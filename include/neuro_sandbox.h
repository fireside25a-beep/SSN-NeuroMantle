#ifndef NEURO_SANDBOX_H
#define NEURO_SANDBOX_H
#ifdef __cplusplus
extern "C" {
#endif
/* Call in a forked child immediately before exec. Returns -1 if isolation cannot be installed. */
/* preserve_fd must be -1 or 3. Descriptor 3 is an explicit Unix stream capability. */
int nm_sandbox_child(int preserve_fd);
#ifdef __cplusplus
}
#endif
#endif
