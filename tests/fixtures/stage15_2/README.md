# Stage 15.2 real-script fixtures

These files are byte-for-byte copies of the user scripts used for the Stage 15.2
compatibility analysis. They are test fixtures only and are never installed into
an IPA.

| File | SHA-256 |
|---|---|
| `arzmod.lua` | `0db5b0ad7d560e920cc6d36b00ad9f649411d1b863a932bb5cc18900f38acbe0` |
| `Helper_for_earning_money.lua` | `00b8e69fb5e3ec9de98bfebffd126795a76a826414aa20c6a5bd138be3a6b12c` |

Verified against the real extracted NeoMLoader payload during Stage 15.2R:

`/data/data/com.termux/files/home/ArizonaNeo-iOS/.research-inputs/neomloader-extracted/neomloader/`

The payload has 300 files and 210 Lua files. Both fixtures are byte-identical to
the corresponding extracted originals (`cmp` exit status 0). The extraction is
not committed and remains outside Git.

`user_script_compat_test` and `script_manager_integration_test` copy these
fixtures into a sandbox and execute their top-level code. `arzmod.lua` uses only
`lib.samp.events` and function definitions at load time.
`Helper_for_earning_money.lua` uses `lib.moonloader` and `lib.samp.events` at
load time. Their `main()` functions call native SAMP/GTA APIs and are not
executed by the tests.

`script_manager_integration_test` also simulates the archive layout: an existing
`lib/samp/events.lua` is present, but `NeoLoaderHost` must select the portable
registry embedded from `scripts/neo/modules/samp/events.lua`.
