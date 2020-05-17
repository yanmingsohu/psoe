void print(char* buf);
void pnumi(int a);
void pf(char* buf, int num);

typedef unsigned char byte;

// 'main' must be first function
int main() {
  char* end = "\n< ----- PSexe Exit ----- >\n\n";
  int a = 0xFF000010;
  int b = 0x03000100;
  long c, d;
  unsigned int e = 0xF0000000;
  char f = 0xff, g = 0x01;
  
  pf("a= ", a);
  pf("b= ", b);
  
  pf("a+b= ", a+b); 
  pf("a-b= ", a-b); 
  
  c = a*b;
  pf("a*b= ", c); 
  d = a/b;
  pf("a/b= ", d); 
  
  pf("a^b= ", a^b);
  pf("a&b= ", a&b);
  pf("a|b= ", a|b);
  
  pf("a<<3 = ", a<<3);
  pf("a>>3 = ", a>>3);
  pf("e>>16 = ", e >> 16);
  pf("e<<2 = ", e << 2);
  
  pf("f+g = ", (byte)(f+g));
  pf("f-g = ", (byte)(f-g));
  
  end[3] = '*';
  print(end);
  return 0;
}


void pf(char* buf, int num) {
  print(buf);
  pnumi(num);
  print("\n");
}


void print(char* buf) {
  unsigned int* PRINTIO = (unsigned int*) 0x1F801050;
  int p = 0;
  char c;
  
  for (c = buf[p]; c!=0; c=buf[++p]) {
    *PRINTIO = c;
  }
}


void pnumi(int a) {
  char c[11] = "0x";
  char* t = "0123456789ABCDEF";
  c[2] = t[(a >> 28) & 0xF];
  c[3] = t[(a >> 24) & 0xF];
  c[4] = t[(a >> 20) & 0xF];
  c[5] = t[(a >> 16) & 0xF];
  
  c[6] = t[(a >> 12) & 0xF];
  c[7] = t[(a >> 8) & 0xF];
  c[8] = t[(a >> 4) & 0xF];
  c[9] = t[(a) & 0xF];
  c[10] = 0;
  print(c);
}


void __main() {
  return;
}  


void *memset(void *str, int c, int n) {
  unsigned char *ch = (unsigned char*) str;
  int i;
  for (i = 0; i < n; ++i) {
    ch[i] = (unsigned char)c;
  }
  return str;
}
