module hex7seg (
    input  logic [3:0] a,
    output logic [6:0] y
);

  always @(a) begin
    case (a)
      4'h0: y = 7'h40;
      4'h1: y = 7'h79;
      4'h2: y = 7'h24;
      4'h3: y = 7'h30;
      4'h4: y = 7'h19;
      4'h5: y = 7'h12;
      4'h6: y = 7'h02;
      4'h7: y = 7'h78;
      4'h8: y = 7'h00;
      4'h9: y = 7'h10;
      4'hA: y = 7'h08;
      4'hB: y = 7'h03;
      4'hC: y = 7'h46;
      4'hD: y = 7'h21;
      4'hE: y = 7'h06;
      4'hF: y = 7'h0E;
      default: y = 7'h7F;
    endcase
  end

endmodule
