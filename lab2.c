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

#define RECV_TOP 1     /* first row for messages (row 0 is asterisks) */
#define RECV_BOTTOM 19 /* last row for messages (row 15 is the divider) */

#define RECV_ROWS (RECV_BOTTOM - RECV_TOP + 1)
#define MAX_MSG_LEN (MAX_COLS + 1)

/* Colors: R, G, B */
#define MY_R 100
#define MY_G 200
#define MY_B 255 /* light blue for my messages */

#define OTHER_R 0
#define OTHER_G 255
#define OTHER_B 100 /* green for others */

#define INPUT_R 255
#define INPUT_G 255
#define INPUT_B 0 /* yellow for typing area */

char recv_buf[RECV_ROWS][MAX_MSG_LEN];
int recv_buf_count = 0; /* how many lines stored */

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
int cursor_visible = 1;
char last_sent[MAX_INPUT_USER + 1];
int has_last_sent = 0;

struct libusb_device_handle *keyboard;
uint8_t endpoint_address;

pthread_t network_thread;

int recv_row = RECV_TOP;
pthread_mutex_t display_mutex = PTHREAD_MUTEX_INITIALIZER;
int skip_next_recv = 0;
pthread_mutex_t skip_mutex = PTHREAD_MUTEX_INITIALIZER;

unsigned char recv_color_r[RECV_ROWS];
unsigned char recv_color_g[RECV_ROWS];
unsigned char recv_color_b[RECV_ROWS];
void *network_thread_f(void *);

void clear_row(int row)
{
  int c;
  for (c = 0; c < MAX_COLS; c++)
    fbputchar(' ', row, c);
}
void scroll_recv(void)
{
  int r;
  for (r = 0; r < RECV_ROWS - 1; r++)
  {
    memcpy(recv_buf[r], recv_buf[r + 1], MAX_MSG_LEN);
    recv_color_r[r] = recv_color_r[r + 1];
    recv_color_g[r] = recv_color_g[r + 1];
    recv_color_b[r] = recv_color_b[r + 1];
  }
  memset(recv_buf[RECV_ROWS - 1], 0, MAX_MSG_LEN);
  recv_color_r[RECV_ROWS - 1] = 255;
  recv_color_g[RECV_ROWS - 1] = 255;
  recv_color_b[RECV_ROWS - 1] = 255;

  for (r = 0; r < RECV_ROWS; r++)
  {
    clear_row(RECV_TOP + r);
    int c;
    for (c = 0; recv_buf[r][c] != '\0' && c < MAX_COLS; c++)
      fbputchar_color(recv_buf[r][c], RECV_TOP + r, c,
                      recv_color_r[r], recv_color_g[r], recv_color_b[r]);
  }
}

void redraw_input(char *buf, int len, int cur)
{
  int i;
  clear_row(INPUT_ROW1);
  clear_row(INPUT_ROW2);
  for (i = 0; i < len; i++)
  {
    int row = INPUT_ROW1 + (i / MAX_COLS);
    int col = i % MAX_COLS;
    fbputchar_color(buf[i], row, col, INPUT_R, INPUT_G, INPUT_B);
  }
  {
    int row = INPUT_ROW1 + (cur / MAX_COLS);
    int col = cur % MAX_COLS;
    fbputchar_color('_', row, col, INPUT_R, INPUT_G, INPUT_B);
  }
}

void display_message_color(const char *msg, unsigned char r, unsigned char g, unsigned char b)
{
  int col = 0;
  int i;

  pthread_mutex_lock(&display_mutex);

  /* Start a new line for this message */
  if (recv_buf_count > 0)
  {
    if (recv_row >= RECV_BOTTOM)
      scroll_recv();
    else
      recv_row++;
  }
  recv_buf_count++;
  memset(recv_buf[recv_row - RECV_TOP], 0, MAX_MSG_LEN);
  recv_color_r[recv_row - RECV_TOP] = r;
  recv_color_g[recv_row - RECV_TOP] = g;
  recv_color_b[recv_row - RECV_TOP] = b;
  clear_row(recv_row);

  for (i = 0; msg[i] != '\0'; i++)
  {
    if (msg[i] == '\n' || msg[i] == '\r')
      continue;

    if (col >= MAX_COLS)
    {
      if (recv_row >= RECV_BOTTOM)
        scroll_recv();
      else
        recv_row++;
      col = 0;
      memset(recv_buf[recv_row - RECV_TOP], 0, MAX_MSG_LEN);
      recv_color_r[recv_row - RECV_TOP] = r;
      recv_color_g[recv_row - RECV_TOP] = g;
      recv_color_b[recv_row - RECV_TOP] = b;
      clear_row(recv_row);
    }

    fbputchar_color(msg[i], recv_row, col, r, g, b);
    recv_buf[recv_row - RECV_TOP][col] = msg[i];
    col++;
  }

  pthread_mutex_unlock(&display_mutex);
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

  for (col = 0; col < 64; col++)
  {
    fbputchar('*', 0, col);
    fbputchar('-', 20, col);
    fbputchar('*', 23, col);
  }

  if ((keyboard = openkeyboard(&endpoint_address)) == NULL)
  {
    fprintf(stderr, "Did not find a keyboard\n");
    exit(1);
  }

  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    fprintf(stderr, "Error: Could not create socket\n");
    exit(1);
  }

  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(SERVER_PORT);
  if (inet_pton(AF_INET, SERVER_HOST, &serv_addr.sin_addr) <= 0)
  {
    fprintf(stderr, "Error: Could not convert host IP \"%s\"\n", SERVER_HOST);
    exit(1);
  }

  if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
  {
    fprintf(stderr, "Error: connect() failed.  Is the server running?\n");
    exit(1);
  }

  pthread_create(&network_thread, NULL, network_thread_f, NULL);

  /* Draw initial cursor */
  redraw_input(input_buf, input_len, cursor_pos);

  for (;;)
  {
    libusb_interrupt_transfer(keyboard, endpoint_address,
                              (unsigned char *)&packet, sizeof(packet),
                              &transferred, 300);
    if (transferred == sizeof(packet))
    {
      uint8_t keycode = packet.keycode[0];

      if (keycode == 0x29)
        break; /* ESC */

      /* Skip release and repeat */
      if (keycode == 0 || keycode == prev_keycode)
      {
        if (keycode == 0)
          prev_keycode = 0;
        continue;
      }
      prev_keycode = keycode;

      /* Always reset cursor to visible on any keypress */
      cursor_visible = 1;

      /* Ctrl + Delete: clear everything */
      if (keycode == 0x4C && (packet.modifiers & (USB_LCTRL | USB_RCTRL)))
      {
        int r;
        for (r = RECV_TOP; r <= RECV_BOTTOM; r++)
          clear_row(r);
        recv_row = RECV_TOP;
        input_len = 0;
        cursor_pos = 0;
        memset(input_buf, 0, sizeof(input_buf));
        redraw_input(input_buf, input_len, cursor_pos);
        continue;
      }

      /* Ctrl + Backspace: clear input only */
      if (keycode == 0x2A && (packet.modifiers & (USB_LCTRL | USB_RCTRL)))
      {
        input_len = 0;
        cursor_pos = 0;
        memset(input_buf, 0, sizeof(input_buf));
        redraw_input(input_buf, input_len, cursor_pos);
        continue;
      }

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

      /* Backspace (no modifier) */
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
        if (input_len > 0)
        {
          input_buf[input_len] = '\0';
          display_message_color(input_buf, MY_R, MY_G, MY_B);

          pthread_mutex_lock(&skip_mutex);
          strncpy(last_sent, input_buf, MAX_INPUT_USER);
          last_sent[MAX_INPUT_USER] = '\0';
          has_last_sent = 1;
          pthread_mutex_unlock(&skip_mutex);

          input_buf[input_len] = '\n';
          write(sockfd, input_buf, input_len + 1);
        }
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
    else
    {
      /* Timeout: blink cursor */
      int row = INPUT_ROW1 + (cursor_pos / MAX_COLS);
      int col = cursor_pos % MAX_COLS;
      if (cursor_visible)
      {
        if (cursor_pos < input_len)
          fbputchar_color(input_buf[cursor_pos], row, col, INPUT_R, INPUT_G, INPUT_B);
        else
          fbputchar(' ', row, col);
      }
      else
      {
        fbputchar('_', row, col);
      }
      cursor_visible = !cursor_visible;
    }
  }

  pthread_cancel(network_thread);
  pthread_join(network_thread, NULL);
  return 0;
}
void *network_thread_f(void *ignored)
{
  char recvBuf[BUFFER_SIZE];
  int n;
  while ((n = read(sockfd, &recvBuf, BUFFER_SIZE - 1)) > 0)
  {
    recvBuf[n] = '\0';
    printf("Recv: %s", recvBuf);

    pthread_mutex_lock(&skip_mutex);
    if (has_last_sent)
    {
      if (strstr(recvBuf, last_sent) != NULL)
      {
        has_last_sent = 0;
        pthread_mutex_unlock(&skip_mutex);
        continue;
      }
    }
    pthread_mutex_unlock(&skip_mutex);

    display_message_color(recvBuf, OTHER_R, OTHER_G, OTHER_B);
  }
  return NULL;
}