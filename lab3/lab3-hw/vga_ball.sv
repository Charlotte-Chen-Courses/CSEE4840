/*
 * Avalon memory-mapped peripheral that generates VGA
 *
 * Stephen A. Edwards
 * Columbia University
 *
 * Register map:
 * 
 * Byte Offset 15    ...    0   Meaning
 *        0    | ball_x_reg |  16-bit ball center X
 *        1    | ball_y_reg |  16-bit ball center Y
 *        2    |     Red    |  Red component (0~7)
 *        3    |     Green  |  Green component (0~7)
 *        4    |     Blue   |  Blue component (0~7)
 */

module vga_ball(input logic        clk,
                input logic        reset,
                input logic [15:0]  writedata,
                input logic        write,
                input logic        chipselect,
                input logic [2:0]  address,

                output logic [7:0] VGA_R, VGA_G, VGA_B,
                output logic       VGA_CLK, VGA_HS, VGA_VS,
                                   VGA_BLANK_n,
                output logic       VGA_SYNC_n);

   parameter BALL_RADIUS = 16;

   logic [15:0] ball_x_reg, ball_y_reg;

   logic [10:0]    hcount;
   logic [9:0]     vcount;

   logic [7:0]     background_r, background_g, background_b;
   logic [15:0] ball_x_disp, ball_y_disp;

   
        
   vga_counters counters(.clk50(clk), .*);

   logic in_vblank;
   assign in_vblank = (vcount >= 10'd480);
   logic vblank_prev;
   logic vblank_rising;
 
   always_ff @(posedge clk)
     if (reset) vblank_prev <= 1'b0;
     else       vblank_prev <= in_vblank;
 
   assign vblank_rising = in_vblank & ~vblank_prev;

   always_ff @(posedge clk)
    if (reset) begin
        ball_x_disp <= 16'd320;
        ball_y_disp <= 16'd240;
    end else if (vblank_rising) begin
        ball_x_disp <= ball_x_reg;
        ball_y_disp <= ball_y_reg;
   end

   always_ff @(posedge clk)
     if (reset) begin
        ball_x_reg   <= 16'd320;
        ball_y_reg   <= 16'd240;
        background_r <= 8'h0;
        background_g <= 8'h0;
        background_b <= 8'h80;
     end else if (chipselect && write)
       case (address)
         3'h0 : ball_x_reg   <= writedata;
         3'h1 : ball_y_reg   <= writedata;
         3'h2 : background_r <= writedata[7:0];
         3'h3 : background_g <= writedata[7:0];
         3'h4 : background_b <= writedata[7:0];
       endcase

   logic signed [10:0] dx, dy;
   logic signed [21:0] dist_sq;
   localparam signed [21:0] RADIUS_SQ = 22'(BALL_RADIUS * BALL_RADIUS);

   logic ball_on;
 
   always_comb begin
      dx = $signed({1'b0, hcount[10:1]}) - $signed({1'b0, ball_x_disp[9:0]});
      dy = $signed({1'b0, vcount})        - $signed({1'b0, ball_y_disp[9:0]});
      dist_sq = dx * dx + dy * dy;
      ball_on = (dist_sq <= RADIUS_SQ);
   end

   always_comb begin
      {VGA_R, VGA_G, VGA_B} = {8'h0, 8'h0, 8'h0};
      if (VGA_BLANK_n) begin
         if (ball_on)
            {VGA_R, VGA_G, VGA_B} = {8'hff, 8'hff, 8'hff};
         else
            {VGA_R, VGA_G, VGA_B} = {background_r, background_g, background_b};
      end
   end
               
endmodule

module vga_counters(
 input logic         clk50, reset,
 output logic [10:0] hcount,  // hcount[10:1] is pixel column
 output logic [9:0]  vcount,  // vcount[9:0] is pixel row
 output logic        VGA_CLK, VGA_HS, VGA_VS, VGA_BLANK_n, VGA_SYNC_n);

/*
 * 640 X 480 VGA timing for a 50 MHz clock: one pixel every other cycle
 * 
 * HCOUNT 1599 0             1279       1599 0
 *             _______________              ________
 * ___________|    Video      |____________|  Video
 * 
 * 
 * |SYNC| BP |<-- HACTIVE -->|FP|SYNC| BP |<-- HACTIVE
 *       _______________________      _____________
 * |____|       VGA_HS          |____|
 */
   // Parameters for hcount
   parameter HACTIVE      = 11'd 1280,
             HFRONT_PORCH = 11'd 32,
             HSYNC        = 11'd 192,
             HBACK_PORCH  = 11'd 96,   
             HTOTAL       = HACTIVE + HFRONT_PORCH + HSYNC +
                            HBACK_PORCH; // 1600
   
   // Parameters for vcount
   parameter VACTIVE      = 10'd 480,
             VFRONT_PORCH = 10'd 10,
             VSYNC        = 10'd 2,
             VBACK_PORCH  = 10'd 33,
             VTOTAL       = VACTIVE + VFRONT_PORCH + VSYNC +
                            VBACK_PORCH; // 525

   logic endOfLine;
   
   always_ff @(posedge clk50 or posedge reset)
     if (reset)          hcount <= 0;
     else if (endOfLine) hcount <= 0;
     else                hcount <= hcount + 11'd 1;

   assign endOfLine = hcount == HTOTAL - 1;
       
   logic endOfField;
   
   always_ff @(posedge clk50 or posedge reset)
     if (reset)          vcount <= 0;
     else if (endOfLine)
       if (endOfField)   vcount <= 0;
       else              vcount <= vcount + 10'd 1;

   assign endOfField = vcount == VTOTAL - 1;

   // Horizontal sync: from 0x520 to 0x5DF (0x57F)
   // 101 0010 0000 to 101 1101 1111
   assign VGA_HS = !( (hcount[10:8] == 3'b101) &
                      !(hcount[7:5] == 3'b111));
   assign VGA_VS = !( vcount[9:1] == (VACTIVE + VFRONT_PORCH) / 2);

   assign VGA_SYNC_n = 1'b0; // For putting sync on the green signal; unused
   
   // Horizontal active: 0 to 1279     Vertical active: 0 to 479
   // 101 0000 0000  1280              01 1110 0000  480
   // 110 0011 1111  1599              10 0000 1100  524
   assign VGA_BLANK_n = !( hcount[10] & (hcount[9] | hcount[8]) ) &
                        !( vcount[9] | (vcount[8:5] == 4'b1111) );

   /* VGA_CLK is 25 MHz
    *             __    __    __
    * clk50    __|  |__|  |__|
    *        
    *             _____       __
    * hcount[0]__|     |_____|
    */
   assign VGA_CLK = hcount[0]; // 25 MHz clock: rising edge sensitive
   
endmodule
