# Sniper Elite Inventory & Weapon System Notes

- "import ida_dbg\n\n# Basic pointers\nip = ida_dbg.get_ip_val()\nsp = ida_dbg.get_sp_val()\ntid = ida_dbg.get_current_thread()\nprint('TID', tid)\nprint('IP', hex(ip) if ip is not None else ip)\nprint('SP', hex(sp) if sp is not None else sp)\n\n# Dump register values (best-effort across IDA versions)\ntry:\n vals = ida_dbg.get_reg_vals(ida_dbg.get_current_thread())\n # ida_dbg.get_reg_vals() returns a 'regvals_t' (iterable) in many versions\n out = []\n for rv in vals:\n # rv: (name, value) style in some builds; or has .name/.ival\n name = getattr(rv, 'name', None)\n if name is None and isinstance(rv, (tuple, list)) and len(rv) >= 2:\n name = rv[0]\n val = rv[1]\n else:\n val = getattr(rv, 'ival', None)\n if val is None:\n val = getattr(rv, 'value', None)\n if name is not None:\n out.append((str(name), val))\n # Print a focused subset first\n preferred = ['EIP','RIP','ESP','RSP','EBP','RBP','EAX','EBX','ECX','EDX','ESI','EDI','EFLAGS','RAX','RBX','RCX','RDX','RSI','RDI','RFLAGS']\n out_dict = {k.upper(): v for k,v in out}\n for k in preferred:\n ku = k.upper()\n if ku in out_dict:\n v = out_dict[ku]\n if isinstance(v, int):\n print(f'{ku} = {hex(v)}')\n else:\n print(f'{ku} = {v}')\n\n # Also show everything we managed to parse\n print('--- ALL PARSED REGS ---')\n for k,v in sorted(out_dict.items()):\n if isinstance(v, int):\n print(f'{k} = {hex(v)}')\n else:\n print(f'{k} = {v}')\nexcept Exception as e:\n print('get_reg_vals failed:', repr(e))\n\n# Also try direct get_reg_val() for common regs (x86)\nfor r in ['EIP','ESP','EBP','EAX','EBX','ECX','EDX','ESI','EDI','EFLAGS']:\n try:\n v = ida_dbg.get_reg_val(r)\n if v is not None:\n print(f'get_reg_val({r}) = {hex(v) if isinstance(v,int) else v}')\n except Exception:\n pass\n"

## Inventory
- Struct base (example: `0x183BF7F8`) embeds `Snipe_Inventory` at `structBase + 0x8`.
- Inventory layout:
  - `+0x00` – vtable.
  - `+0x04` – array of 16-bit item counts (`SNIPE_NUM_ITEM_TYPES = 40`).
  - `+0x54` – head pointer of the circular item-node list.
- `sub_511070` (`0x511070`) decrements counts and notifies the weapon system; it expects `ECX = inventory` and `a2` in the `0..39` range.

## Item list
- The `Snipe_Weapon_System` keeps a static name lookup table in `.rdata` starting at `0x726400`. Each entry is a 4-byte pointer to a null-terminated string;
- Below is the runtime ID list of weapons.

HEX ID | Notes                          |
------ | ------------------------------ |
0x01   | Pistol ammo                    |
0x02   | Rifle ammo                     |
0x03   | PPSH ammo                      |
0x04   | MP-40 ammo                     |
0x05   | MG40 ammo                      |
0x06   | DP-28 ammo                     |
0x08   | Panzerfaust                    |
0x09   | Stick grenade                  |
0x0A   | Frag grenade                   |
0x0B   | Smoke grenade                  |
0x0C   | Knife                          |
0x0D   | Medkit                         |
0x0E   | Bandage                        |
0x0F   | TNT/Mine                       |
0x11   | Binoculars                     |
0x13   | Gewehr43                       |
0x14   | Mauser                         |
0x15   | SVT-40                         |
0x16   | Luger                          |
0x17   | P-38                           |
0x18   | PPSH                           |
0x19   | MP-40                          |
0x1A   | MG40                           |
0x1B   | DP-28                          |
0x1C   | Time Bomb                      |
0x1D   | Panzerschreck                  |
0x1E   | Tripwire grenade               |
0x1F   | Mauser without scope           |
0x20   | Nagant without scope           |
0x21   | Unknown / unused               |
0x22   | Panzerfaust (Empty)            |
0x24   | Unknown / unused               |
0x26   | Panzerschreck ammo             |
0x27   | Dogtag cross                   |

- Remaining entries include clothing/headgear (`gcap`, `ghelm`, `sshelm`, etc.), backpacks, vehicle parts, and other props;

## Hook functions
- Identified that `sub_512020` is the core `Snipe_Inventory::AddItems` routine and mapped its four callers:
  1. `sub_5125E0` – chunk loader that hydrates every entity inventory from serialized data.
  2. `sub_518E40` – generic entity handler that updates HUD/listeners for types not equal to 39.
  3. `sub_519C90` – player-specific wrapper (offset `this+8`) that also triggers UI/network events.
  4. `sub_519E60` – NPC wrapper that notifies owner AI after updating counts.
- `0x519C90 (InventoryAssign)` – intercepts all player item grants so we can log arguments, swap item IDs (e.g., force SVT-40), and retain the latest `this` pointer.
- `0x51AF80 (InventoryUpdate)` – fires every inventory tick.
- Captured pointers are reused by a small debugging loop:
  - **F7** → dumps the current inventory contents. Implementation mirrors the in-game loop: treat `inventoryBase + 8` as the start of the 16-bit quantity array, iterate offsets `6..82` stepping by 2, and compute `itemId = (offset - 6)/2 + 1`.
- Console output examples:
  - `Local player inventory: 0x170BF4E8`
  - `Item ID: 0x15 -> Quantity: 1`

## Player instrumentation
- **Nickname payload (`0x6080C0`)** – this routine is invoked whenever the server sends client info. The data block we hook has the following confirmed offsets: `+0x00` unknown (8 bytes), `+0x08` wide nickname (16 characters), `+0x28` padding (20 bytes), `+0x3C` uID (`uint32_t`), and `+0x40` join flag (`uint8_t`, `1` = joining, `0` = leaving). A detour prints `Player <name> joined/left (uID, inventory)` and tags `[LOCAL]` when applicable.
- **Player constructor (`0x56F390`)** – every player object stores a pointer to its inventory at offset `+0x1E8` (488 bytes). Hooking the constructor lets us capture that pointer and map `uID → inventory` inside a `std::map`. These mappings remain valid even after the introductory handshake completes.
- **Inventory lookup** – by combining the constructor map with the nickname hook we can print the inventory address for any uID as soon as the join packet lands. When a player disconnects we erase the entry to keep the map clean.
- **Diagnostics** – `InventoryAssign` logging is still active and prints the inventory address plus weapon parameters whenever the game grants items, which helps confirm that the uID→inventory association is correct.

## Player Connection and IP Discovery
- **Packet Analysis**: By tracing the `recvfrom` function, the core packet handler `sub_641750` was identified. This function dispatches packets based on their type and the sender's connection status.
- **Player Handshake**: A specific code path was found for handling packets from new, unrecognized IP addresses. This path leads to the function `sub_641D60`.
- **Player Join Event Hook (`0x641D60`)**: This function is the core handler for new player connections. It is responsible for:
  1. Allocating a new low-level network object for the connecting player.
  2. Initializing this object with the player's IP address and port, which are passed as arguments.
  3. Adding the new player to the server's internal lists.
- **Implementation**: A hook was placed on `sub_641D60` (`PlayerIPListener_Detour`). This hook intercepts new player connections as they happen, providing a reliable way to capture the IP address and port for each player who joins the server. This new method works correctly on GameSpy LAN servers and replaces the previous, less reliable iteration method.

## Team Deathmatch Autobalance Analysis
- **Core logic**: Mapped to a logic block starting at `0x617E80` (part of the virtual function at `0x617E20`).
- **Detection**: Compares player counts between teams. If uneven, it initializes a 30-second timer at offset `+0x150` of the TDM management object (base at `0x75DDF8`).
- **Messaging**: Uses the server broadcast system `sub_4FFCE0`.
  - **Message ID 62 (0x3E)**: "Auto team balance in 30 seconds."
  - **Parameter 30 (0x1E)**: Time remaining.
- **Bypass**: By detouring `0x617E20` or patching the jump at `0x617E99`, the entire balancing sequence (including the announcement) can be suppressed.

## Multiplayer: Raising the 8-player limit (32 slots)

### Overview (what was limiting us)
- The game has *multiple* independent limits:
  - **Gameplay join validation**: A server-side cap that rejected the 9th join even when the network layer was configured for 32.
  - **Client-side network init**: some client code paths re-initialize networking with `8` and can desync client/host expectations.

### Key functions and checks we traced (IDA)

#### Network accept gate (host)
- **`sub_447240`**: Connection accept/reject handler.
  - Rejects as "server full" if `dword_781520 >= dword_7814D4`.
  - Also validates:
    - struct size (`a7 == 8`)
    - incoming player count (`dword_781514 + a6[1] <= dword_7814F0`)

#### Slot broadcast
- **`sub_60D360`**: Copies the server options blob into live globals (including the max player count) and is used as the broadcast/update path.
- The relevant field for "max players" in the options is at **`options + 0x50`**.

#### Gameplay join hard cap (server)
- **`sub_614900`**: Server-side join validation.
- The 8-player limiter is here:
  - **`0x6149CA`**: `cmp eax, 8` (where `eax = connected + requested`)
  - On the 9th join attempt we observed `EAX = 9`, causing rejection with code `111` ("Ran out of client histories").

### Notes / limitations
- If you see clients resetting back to `8`, revisit the client-side callsites that pass `8,8` into network initialization (`sub_452E80` callsites). These were observed during debugging but are not necessarily patched in the current DLL build.

### Note: Recent Progress (Raising Player Limit to 32)

We have successfully expanded the player limit from 8 to 32. This involved deep hooks into the game's "Client History" system and addressing several hard-coded limits.

#### 1. Client History Expansion
- **Allocation**: Redirected the static `ClientHistoryEntry` array (originally at `0x804B78`, size 8) to a dynamically allocated heap array of 32 entries.
- **Initialization**: Hooked `sub_60C840` (constructor) and `sub_60BE00` (init) to handle 32 entries instead of 8.
- **Function Detours**: Replaced several core management functions to support the expanded array:
    - `ClientHistoryCountConnected` (`0x60C0C0`)
    - `ClientHistoryFindByNumber` (`0x60C120`)
    - `ClientHistoryIndexByNumber` (`0x60C190`)
    - `ClientHistoryIterNextConnected` (`0x60C290`)
    - `ClientHistoryClearByNumber` (`0x60C930`)
    - `ClientHistoryAddOrUpdate` (`0x60C9F0`)

#### 2. Fixes for Disconnect/Reconnect
- **Mission Reset Cleanup**: Modified `ClientHistoryClearByNumber` (`0x60C930`) to ensure all 32 slots are properly cleared/reset during map restarts or round changes. This fixed the issue where players 9-32 were "stuck" in the history and couldn't reconnect.

#### 3. Ongoing Investigation: Scoring & Leaderboards
- **The Problem**: While 32 players can play, the leaderboard remains empty and scores do not update for players beyond the original 8.
- **Findings**: The score system uses its own internal static arrays (e.g., `dword_7AE6D4`, `byte_7AE734`) and mapping tables (`unk_7E2ED8`) which are still hard-coded for 8 entries.
- **Target Functions**:
    - `sub_4AA330`: Score system initialization (allocates 8 slots).
    - `sub_4A9270`: Score message handler (maps player numbers to 8 slots).
    - `sub_4A9100`, `sub_4A91D0`: Scoreboard aggregation functions (looping only to 8).
- **Next Steps**: Patch or hook the score system to expand its internal slot count to 32 and update the mapping logic.

