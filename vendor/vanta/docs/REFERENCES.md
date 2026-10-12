# Implementation references

- [Reproducible Builds: SOURCE_DATE_EPOCH specification](https://reproducible-builds.org/specs/source-date-epoch/) informs the fixed metadata in release archives. Executable reproducibility is checked separately by repeated byte comparison.
- [GitHub Actions secure use reference](https://docs.github.com/en/actions/reference/security/secure-use) informs workflow token permissions. No release publication token is used by the verification job.
- [Arm ABI specifications](https://github.com/ARM-software/abi-aa/releases) specify the target C ABI and ELF conventions consumed by the AArch64 compiler in the source-to-C backend. Its executable verification is defined in the AArch64 CI job; the local x86-64 run does not establish an AArch64 pass.
