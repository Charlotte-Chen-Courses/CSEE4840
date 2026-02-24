module collatz (
    input  logic        clk,   // Clock
    input  logic        go,    // Load value from n; start iterating
    input  logic [31:0] n,     // Start value; only read when go = 1
    output logic [31:0] dout,  // Iteration value: true after go = 1
    output logic        done
);  // True when dout reaches 1

  assign done = (dout == 32'd1);
  always_ff @(posedge clk) begin
    if (go) begin
      dout <= n;
    end else begin
      if (dout != 1) begin
        if (dout[0] == 0) begin
          dout <= dout >> 1;
        end else begin
          dout <= 3 * dout + 1;
        end
      end
    end
  end

endmodule
