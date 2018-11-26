# GPIO device driver for studying

# Compile

```bash
$ make
```

# Sample

```cpp
nclude <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define DEVICE_NAME "/dev/my_device_-2143289282"

struct ioctl_data {
    int mode;
    unsigned int pin_number;
};

int main(void)
{
  int file_descriptor;
  if ((file_descriptor = open(DEVICE_NAME, O_RDWR | O_SYNC)) < 0) {
    perror("failed to open()\n");
    return -1;
  }

  struct ioctl_data data = {
    .mode = 1, // write
    .pin_number = 4,
  };
  ioctl(file_descriptor, 0, &data);
  
  char* mode = "0";
  write(file_descriptor, mode, sizeof(char*));

  close(file_descriptor);
  return 0;
}
```
