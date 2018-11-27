#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define DEVICE_NAME "/dev/my_gpio_device"

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
    .mode = 1, // out
    .pin_number = 4,
  };
  ioctl(file_descriptor, 0, &data);
  
  char order[1];
  char state[1];

  order[0] = '1';
  write(file_descriptor, order, sizeof(order));
  read(file_descriptor, state, sizeof(state));
  printf("state is %s\n", state);
  
  sleep(1);
  
  order[0] = '0';
  write(file_descriptor, order, sizeof(order));
  read(file_descriptor, state, sizeof(state));
  printf("state is %s\n", state);

  close(file_descriptor);
  return 0;
}
