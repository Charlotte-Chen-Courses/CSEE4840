/*
 *
 * CSEE 4840 Lab 2 for 2019
 *
 * Name/UNI: Please Changeto Yourname (pcy2301)
 */
#include "fbputchar.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "usbkeyboard.h"
#include <pthread.h>

/* Update SERVER_HOST to be the IP address of
 * the chat server you are connecting to
 */
/* arthur.cs.columbia.edu */
#define SERVER_HOST "128.59.19.114"
#define SERVER_PORT 42000
#define INPUT_ROW1 21
#define INPUT_ROW2 22
#define MAX_COLS 64
#define MAX_INPUT_USER (MAX_COLS * 2) /* two rows of input */
#define BUFFER_SIZE 128

/*
 * References:
 *
 * https://web.archive.org/web/20130307100215/http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html
 *
 * http://www.thegeekstuff.com/2011/12/c-socket-programming/
 *
 */

int sockfd; /* Socket file descriptor */

char input_buf[MAX_INPUT_USER + 1];
int input_len = 0;
int cursor_pos = 0;
uint8_t prev_keycode = 0;

struct libusb_device_handle *keyboard;
uint8_t endpoint_address;

pthread_t network_thread;
void *network_thread_f(void *);

void draw_cursor(int row, int col)
{
  fbputchar('_', row, col + 1);
}

void erase_cursor(int row, int col)
{
  fbputchar(' ', row, col + 1);
}

void redraw_input(char *buf, int len, int cur)
{
  int i;
  /* Clear both input rows */
  for (i = 0; i < MAX_COLS; i++)
  {
    fbputchar(' ', INPUT_ROW1, i);
    fbputchar(' ', INPUT_ROW2, i);
  }
  /* Draw text */
  for (i = 0; i < len; i++)
  {
    int row = INPUT_ROW1 + (i / MAX_COLS);
    int col = i % MAX_COLS;
    fbputchar(buf[i], row, col);
  }
  /* Draw cursor */
  {
    int row = INPUT_ROW1 + (cur / MAX_COLS);
    int col = cur % MAX_COLS;
    if (col == 0 && row > INPUT_ROW2)
    {
      row--;
      col = MAX_COLS;
    }
    fbputchar('_', row, col);
  }
}

char keycode_to_ascii(uint8_t keycode, uint8_t modifiers)
{
  int shift = (modifiers & (USB_LSHIFT | USB_RSHIFT)) ? 1 : 0;

  /* Letters: keycodes 0x04 ('a') through 0x1D ('z') */
  if (keycode >= 0x04 && keycode <= 0x1D)
  {
    char c = 'a' + (keycode - 0x04);
    if (shift)
      c = c - 'a' + 'A';
    return c;
  }

  /* Numbers and their shifted symbols */
  if (keycode >= 0x1E && keycode <= 0x27)
  {
    if (!shift)
    {
      /* 1-9, then 0 */
      if (keycode <= 0x26)
        return '1' + (keycode - 0x1E);
      else
        return '0';
    }
    else
    {
      /* Shifted: !@#$%^&*() */
      const char shifted[] = "!@#$%^&*()";
      return shifted[keycode - 0x1E];
    }
  }

  /* Common keys */
  switch (keycode)
  {
  case 0x28:
    return '\n'; /* Enter */
  case 0x2C:
    return ' '; /* Space */
  case 0x2A:
    return '\b'; /* Backspace */
  case 0x2B:
    return '\t'; /* Tab */
  case 0x2D:
    return shift ? '_' : '-';
  case 0x2E:
    return shift ? '+' : '=';
  case 0x2F:
    return shift ? '{' : '[';
  case 0x30:
    return shift ? '}' : ']';
  case 0x31:
    return shift ? '|' : '\\';
  case 0x33:
    return shift ? ':' : ';';
  case 0x34:
    return shift ? '"' : '\'';
  case 0x35:
    return shift ? '~' : '`';
  case 0x36:
    return shift ? '<' : ',';
  case 0x37:
    return shift ? '>' : '.';
  case 0x38:
    return shift ? '?' : '/';
  }

  return 0;
}

int main()
{
  int err, col;

  struct sockaddr_in serv_addr;

  struct usb_keyboard_packet packet;
  int transferred;

  if ((err = fbopen()) != 0)
  {
    fprintf(stderr, "Error: Could not open framebuffer: %d\n", err);
    exit(1);
  }
  fbclear();

  /* Draw rows of asterisks across the top and bottom of the screen */
  for (col = 0; col < 64; col++)
  {
    fbputchar('*', 0, col);
    fbputchar('-', 20, col);
    fbputchar('*', 23, col);
  }

  fbputs("Testing", 4, 10);

  /* Open the keyboard */
  if ((keyboard = openkeyboard(&endpoint_address)) == NULL)
  {
    fprintf(stderr, "Did not find a keyboard\n");
    exit(1);
  }

  /* Create a TCP communications socket */
  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    fprintf(stderr, "Error: Could not create socket\n");
    exit(1);
  }

  /* Get the server address */
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(SERVER_PORT);
  if (inet_pton(AF_INET, SERVER_HOST, &serv_addr.sin_addr) <= 0)
  {
    fprintf(stderr, "Error: Could not convert host IP \"%s\"\n", SERVER_HOST);
    exit(1);
  }

  /* Connect the socket to the server */
  if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
  {
    fprintf(stderr, "Error: connect() failed.  Is the server running?\n");
    exit(1);
  }

  /* Start the network thread */
  pthread_create(&network_thread, NULL, network_thread_f, NULL);

  for (;;)
  {
    libusb_interrupt_transfer(keyboard, endpoint_address,
                              (unsigned char *)&packet, sizeof(packet),
                              &transferred, 0);
    if (transferred == sizeof(packet))
    {
      uint8_t keycode = packet.keycode[0];

      /* ESC pressed? */
      if (keycode == 0x29)
        break;

      /* Skip release and repeat */
      if (keycode == 0 || keycode == prev_keycode)
      {
        if (keycode == 0)
          prev_keycode = 0;
        continue;
      }
      prev_keycode = keycode;

      /* Left arrow */
      if (keycode == 0x50)
      {
        if (cursor_pos > 0)
          cursor_pos--;
        redraw_input(input_buf, input_len, cursor_pos);
        continue;
      }

      /* Right arrow */
      if (keycode == 0x4F)
      {
        if (cursor_pos < input_len)
          cursor_pos++;
        redraw_input(input_buf, input_len, cursor_pos);
        continue;
      }

      /* Backspace */
      if (keycode == 0x2A)
      {
        if (cursor_pos > 0)
        {
          int i;
          for (i = cursor_pos - 1; i < input_len - 1; i++)
            input_buf[i] = input_buf[i + 1];
          input_len--;
          cursor_pos--;
          redraw_input(input_buf, input_len, cursor_pos);
        }
        continue;
      }

      /* Enter */
      if (keycode == 0x28)
      {
        input_buf[input_len] = '\n';
        write(sockfd, input_buf, input_len + 1);
        /* TODO: also display sent message in receive area */
        input_len = 0;
        cursor_pos = 0;
        memset(input_buf, 0, sizeof(input_buf));
        redraw_input(input_buf, input_len, cursor_pos);
        continue;
      }

      /* Printable character */
      char ch = keycode_to_ascii(keycode, packet.modifiers);
      if (ch && ch != '\n' && ch != '\b' && ch != '\t' && input_len < MAX_INPUT_USER - 1)
      {
        int i;
        for (i = input_len; i > cursor_pos; i--)
          input_buf[i] = input_buf[i - 1];
        input_buf[cursor_pos] = ch;
        input_len++;
        cursor_pos++;
        redraw_input(input_buf, input_len, cursor_pos);
      }
    }
  }

  /* Terminate the network thread */
  pthread_cancel(network_thread);

  /* Wait for the network thread to finish */
  pthread_join(network_thread, NULL);

  return 0;
}

void *network_thread_f(void *ignored)
{
  char recvBuf[BUFFER_SIZE];
  int n;
  /* Receive data */
  while ((n = read(sockfd, &recvBuf, BUFFER_SIZE - 1)) > 0)
  {
    recvBuf[n] = '\0';
    printf("%s", recvBuf);
    fbputs(recvBuf, 8, 0);
  }

  return NULL;
}
