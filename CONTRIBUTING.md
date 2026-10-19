Any contributions should follow the guidelines outlined below.

Note that I will ignore any and all contributions that appear to use LLMs/AI.

For issues/bug reports, try to provide:

- The current version that experiences the issue
  - Current version information can be seen via the 'Thanks' page 
  - Hold Z on port 4 on the main menu to show it
- If applicable, the oldest version that also experiences the issue
- The platform used (GameCube, Wii, Dolphin). For Dolphin, also provide the version used
- Instructions for reproducing the issue, if possible

Pull requests are only accepted on the main Codeberg repository. 
Github PRs are disabled in an effort to stop low-effort/automated submissions. 

If I accept a PR, it will likely be merged via a patch, and not through the web interface.
(I want practice with using git command line :) )

For pull requests:

- **Do not use ANY form of LLM/AI code assist/generation.** 
  This INCLUDES "vibe coding", formatting/refactoring, bug fix scanning, etc.
- Maintain current code style (I know it's a mess but there's a general-ish style present)
- Commits should be focused on a specific change, with a corresponding message
- Commits should not be overly large
- Code should be readable and commented
  (use a more readable implementation over a more 'elegant' solution, if possible)
- Changes should not produce any compiler warnings
- Changes should be implemented in C, not C++
- Changes will be licensed under GNU GPLv3
- Changes should not rely on: 
  - Older/specific versions of libogc2 or any utilities from the DevkitPro or Libogc2 repos
  - Tools/libraries not present in either the DevkitPro or Libogc2 repos
- Conversely, changes should target the latest version (upstream) libogc2