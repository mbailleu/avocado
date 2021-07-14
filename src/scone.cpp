#if defined (__cplusplus)
extern "C" {
#endif //__cplusplus

void outl_p(unsigned int value, unsigned short int port) {
  asm volatile ("outl %0, %w1\n"
                "outb %%al, $0x80"
                :
                : "a" (value), "Nd" (port));
}

void outw_p(unsigned short value, unsigned short int port) {
  asm volatile ("outw %0, %w1\n"
                "outb %%al,$0x80"
                :
                : "a" (value), "Nd" (port));
}

void outb_p(unsigned char value, unsigned short int port) {
  asm volatile ("outb %0, %w1\n"
                "outb %%al,$0x80"
                :
                : "a" (value), "Nd" (port));
}

#if defined (__cplusplus)
}
#endif //__cplusplus
