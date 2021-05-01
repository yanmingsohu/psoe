#pragma once

namespace ps1e {



class FrontIO {
private:
  // 1F801044h 
  bool tx_started;      //0
  bool rx_empty;        //1
  bool tx_finished;     //2
  bool ask_input_level; //7
  u32  baud_timer;      //11-31

  // 1F801048h 
  u8   baud_mul;        //0-1
  u8   char_length;     //2-3
  bool parity_enable;   //4
  bool parity_type_odd; //5
  u8   clk_polarity;    //8

  // 1F80104Ah
  bool tx_enable;       //0
  bool joy_output;      //1
  bool rx_enable;       //2
  bool ask_irq;         //4
  bool reset;           //6
  u8   rx_irq_mode;     //8-9
  bool tx_irq_enb;      //10
  bool rx_irq_enb;      //11
  bool ask_irq_enb;     //12
  bool isslot1;         //13

};
}