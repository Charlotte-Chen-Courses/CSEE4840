/*
 * Userspace program that communicates with the vga_ball device driver
 * to display a bouncing ball.
 *
 * Based on skeleton by Stephen A. Edwards, Columbia University
 */

#include <stdio.h>
#include "vga_ball.h"
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

int vga_ball_fd;

/* Screen dimensions and ball radius (must match hardware) */
#define SCREEN_W  640
#define SCREEN_H  480
#define BALL_R    16

/* Set the ball position */
void set_ball_position(unsigned short x, unsigned short y)
{
  vga_ball_arg_t vla;
  vla.position.x = x;
  vla.position.y = y;
  if (ioctl(vga_ball_fd, VGA_BALL_WRITE_POSITION, &vla)) {
      perror("ioctl(VGA_BALL_WRITE_POSITION) failed");
      return;
  }
}

/* Set the background color */
void set_background_color(const vga_ball_color_t *c)
{
  vga_ball_arg_t vla;
  vla.background = *c;
  if (ioctl(vga_ball_fd, VGA_BALL_WRITE_BACKGROUND, &vla)) {
      perror("ioctl(VGA_BALL_WRITE_BACKGROUND) failed");
      return;
  }
}

int main()
{
  static const char filename[] = "/dev/vga_ball";

  printf("VGA ball Userspace program started\n");

  if ((vga_ball_fd = open(filename, O_RDWR)) == -1) {
    fprintf(stderr, "could not open %s\n", filename);
    return -1;
  }

  /* Set a nice background color */
  vga_ball_color_t bg = { 0x00, 0x00, 0x80 };  /* dark blue */
  set_background_color(&bg);

  /* Ball starting position and velocity */
  int x = SCREEN_W / 2;
  int y = SCREEN_H / 2;
  int dx = 3;
  int dy = 2;

  printf("Bouncing ball: radius=%d, starting at (%d,%d)\n", BALL_R, x, y);

  while (1) {
    /* Update position */
    x += dx;
    y += dy;

    /* Bounce off walls (keep ball fully on screen) */
    if (x - BALL_R <= 0) {
      x = BALL_R;
      dx = -dx;
    } else if (x + BALL_R >= SCREEN_W - 1) {
      x = SCREEN_W - 1 - BALL_R;
      dx = -dx;
    }

    if (y - BALL_R <= 0) {
      y = BALL_R;
      dy = -dy;
    } else if (y + BALL_R >= SCREEN_H - 1) {
      y = SCREEN_H - 1 - BALL_R;
      dy = -dy;
    }

    set_ball_position((unsigned short)x, (unsigned short)y);

    /* ~60 fps: 1/60 sec ≈ 16666 us */
    usleep(16666);
  }

  printf("VGA BALL Userspace program terminating\n");
  return 0;
}