// CSEE 4840 Lab 1: Run and Display Collatz Conjecture Iteration Counts
//
// Spring 2026
//
// By: Charlotte Chen
// Uni: HC3558

module lab1 (
    input logic CLOCK_50,  // 50 MHz Clock input

    input logic [3:0] KEY,  // Pushbuttons; KEY[0] is rightmost

    input logic [9:0] SW,  // Switches; SW[0] is rightmost

    // 7-segment LED displays; HEX0 is rightmost
    output logic [6:0] HEX0,
    HEX1,
    HEX2,
    HEX3,
    HEX4,
    HEX5,

    output logic [9:0] LEDR  // LEDs above the switches; LED[0] on right
);

  logic clk, go, done;
  logic [31:0] start;
  logic [15:0] count;

  logic [11:0] n;
  logic [3:0] hundreds_in, tens_in, ones_in, hundreds_out, tens_out, ones_out;

  logic [21:0] pulse_cnt;
  logic [ 7:0] offset;

  localparam REPEAT_LIMIT = 13000000;

  logic [23:0] cnt0, cnt1;
  logic key0_last, key1_last;

  assign clk = CLOCK_50;

  range #(256, 8) // RAM_WORDS = 256, RAM_ADDR_BITS = 8)
         r (
      .*
  );  // Connect everything with matching names


  hex7seg hundreds (
      .a(hundreds_in),
      .y(HEX5)
  );
  hex7seg tens (
      .a(tens_in),
      .y(HEX4)
  );
  hex7seg ones (
      .a(ones_in),
      .y(HEX3)
  );

  hex7seg hundreds_result (
      .a(hundreds_out),
      .y(HEX2)
  );
  hex7seg tens_result (
      .a(tens_out),
      .y(HEX1)
  );
  hex7seg ones_result (
      .a(ones_out),
      .y(HEX0)
  );

  // Replace this comment and the code below it with your own code;
  // The code below is merely to suppress Verilator lint warnings

  assign n = {2'b00, SW} + offset;
  assign hundreds_in = n / 256;
  assign tens_in = (n / 16) % 16;
  assign ones_in = n % 16;


  always_ff @(posedge clk) begin

    key0_last <= KEY[0];
    key1_last <= KEY[1];

    if (KEY[0]) cnt0 <= 0;
    else if (cnt0 < REPEAT_LIMIT) cnt0 <= cnt0 + 1;
    else cnt0 <= 0;

    if (!KEY[3]) offset <= 0;


    if (KEY[1]) cnt1 <= 0;
    else if (cnt1 < REPEAT_LIMIT) cnt1 <= cnt1 + 1;
    else cnt1 <= 0;

    if (!KEY[2]) begin
      offset <= 0;
    end else begin
      if (((key0_last && !KEY[0]) || (cnt0 == REPEAT_LIMIT)) && KEY[1]) begin
        if (offset < 255) begin
          offset <= offset + 1;
        end
      end
      if (((key1_last && !KEY[1]) || (cnt1 == REPEAT_LIMIT)) && KEY[0]) begin
        if (offset > 0) begin
          offset <= offset - 1;
        end
      end
    end
  end

  always_ff @(posedge clk) begin
    if (!KEY[3]) begin
      go <= 1;
      start <= {2'b00, SW};
      LEDR <= 10'b1111111111;
    end else begin
      go   <= 0;
      LEDR <= 10'b0000000000;
      if (done && n != 0) begin
        start <= offset;
        hundreds_out <= count / 256;
        tens_out <= (count / 16) % 16;
        ones_out <= count % 16;
      end else if (n == 0) begin
        hundreds_out <= 4'he;
        tens_out <= 4'he;
        ones_out <= 4'he;
      end
    end
  end






endmodule
