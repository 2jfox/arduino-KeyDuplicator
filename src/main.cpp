#include <Arduino.h>
#include <OneWire.h>
#include "header.h"
#include "pitches.h"

void setup(){
  pinMode(BtnPin, INPUT_PULLUP);
  //pinMode(BtnPinGnd, OUTPUT); digitalWrite(BtnPinGnd, LOW); // подключаем второй пин кнопки к земле
  pinMode(speakerPin, OUTPUT);
  //pinMode(speakerPinGnd, OUTPUT); digitalWrite(speakerPinGnd, LOW); // подключаем второй пин спикера к земле
  pinMode(ACpin, INPUT);                                            // Вход аналогового компаратора для ключей RFID и аналоговых ключей Cyfral / Metacom
  //pinMode(VRpinGnd, OUTPUT); digitalWrite(VRpinGnd, LOW);           // подключаем пин подстроечного резистора к земле
  pinMode(R_Led, OUTPUT); pinMode(G_Led, OUTPUT); pinMode(B_Led, OUTPUT);  //RGB-led
  clearLed();
  pinMode(FreqGen, OUTPUT);                               
  digitalWrite(B_Led, HIGH);                                //awaiting of origin key data
  Serial.begin(115200);
  while(!Serial);  
  Serial.println(F("START " __FILE__ " from " __DATE__));
  Sd_StartOK();
}

void loop_(){
  //read attached ibutton
  if (!ibutton.search(addr)){
    ibutton.reset_search();
    delay(200);
    return;
  }

  //print the buffer content in LSB. For MSB: for (int i = 8; i > 0; i--)
  byte crc = ibutton.crc8(addr, 7);
  sprintf(printBuffer, "> ID: %02X %02X %02X %02X %02X %02X %02X %02X [%02X]",
    addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7], crc);
  Serial.println(printBuffer);  

  Serial.println("***********TEST************");
  //ibutton.skip(); 
  if(!ibutton.reset()) return;
  ibutton.write(0x33); //read CMD
     
  //read the attached ibutton
  ibutton.read_bytes(addr, 8);
  sprintf(printBuffer, "ID: %02X %02X %02X %02X %02X %02X %02X %02X\n",
    addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7]);
  Serial.print(printBuffer);  
  
  return;

    ibutton.skip();
    ibutton.reset();    
    ibutton.write(0xD1);    
    // send logical 0
    digitalWrite(iButtonPin, LOW); pinMode(iButtonPin, OUTPUT); delayMicroseconds(60);
    pinMode(iButtonPin, INPUT); digitalWrite(iButtonPin, HIGH); delay(10);
        
    Serial.print("\nWriting iButton ID:");
                      
    ibutton.skip();
    ibutton.reset();
    ibutton.write(0xD5);//write CMD
    //ibutton.write_bytes(keyID, 8/*sizeof(keyID)/sizeof(keyID[0])*/);
    for (byte i = 0; i < 8; i++){
      writeByte(keyID[i]);
      Serial.print('*');
    }

    ibutton.reset();    
    ibutton.write(0xD1);
    //send logical 1
    digitalWrite(iButtonPin, LOW); pinMode(iButtonPin, OUTPUT); delayMicroseconds(10);
    pinMode(iButtonPin, INPUT); digitalWrite(iButtonPin, HIGH); delay(10);
}

void loop(){
  bool BtnClick, BtnPinSt = digitalRead(BtnPin);
  BtnClick = BtnPinSt != preBtnPinSt;
  preBtnPinSt = BtnPinSt;

  if (BtnClick) {                         // переключаель режима чтение/запись
    if (readflag) {      
      writeflag = !writeflag;
      clearLed(); 
      if (writeflag) digitalWrite(R_Led, HIGH);
      else digitalWrite(G_Led, HIGH);
      sprintf(printBuffer, "RW mode: %s", (writeflag ? "writting" : "reading"));
      Serial.println(printBuffer);
      //Serial.print("RW mode = "); Serial.println(writeflag);  
    } else Sd_ErrorBeep();
  }
  if (!writeflag){
    if (/*searchCyfral() || searchEM_Marine() ||*/ searchIbutton()){
      digitalWrite(G_Led, LOW);
      Sd_ReadOK();
      readflag = true;
      clearLed(); digitalWrite(G_Led, HIGH);
    }
    else {
      delay(100);
      return;
    } 
  }  
  
  if (writeflag && readflag){
    if (keyType == keyEM_Marine) write2rfid();
    else write2iBtn();
  }
  delay(200);
}

int writeByte(byte data){  
  for(byte i = 0; i < 8; i++){
    if (data & 1){
      digitalWrite(iButtonPin, LOW); pinMode(iButtonPin, OUTPUT); delayMicroseconds(60);
      pinMode(iButtonPin, INPUT); digitalWrite(iButtonPin, HIGH);
    } else {
      digitalWrite(iButtonPin, LOW); pinMode(iButtonPin, OUTPUT); /*delayMicroseconds(10);*/
      pinMode(iButtonPin, INPUT);  digitalWrite(iButtonPin, HIGH);
    }
    delay(10);
    data = data >> 1;
  }
  return 0;
}

void writeBit(bool bit){
  digitalWrite(iButtonPin, LOW); pinMode(iButtonPin, OUTPUT); delayMicroseconds(bit? 60:10);
  pinMode(iButtonPin, INPUT); digitalWrite(iButtonPin, HIGH); delay(10);
}

void clearLed(){
  digitalWrite(R_Led, LOW);
  digitalWrite(G_Led, LOW);
  digitalWrite(B_Led, LOW);  
}

/************************** DALLAS (iButton) **************************/

/**/
bool searchIbutton(){
  if (!ibutton.search(addr)) { 
    ibutton.reset_search(); 
    return false;
  }
  
  memcpy(keyID, addr, sizeof(keyID));
  sprintf(printBuffer, "%02X %02X %02X %02X %02X %02X %02X %02X ", 
    keyID[0], keyID[1], keyID[2], keyID[3], keyID[4], keyID[5], keyID[6], keyID[7]);
  Serial.print(printBuffer);

  if (addr[0] == 0x01) {                         // это ключ формата dallas
    keyType = keyDS1990A;
    if (getRWtype() == TM2004) keyType = keyTM2004;
    if (OneWire::crc8(addr, 7) != addr[7]) {
      Serial.println("CRC is not valid!");
      Sd_ErrorBeep();
      digitalWrite(B_Led, HIGH);
      return false;
    }
    return true;
  }
  if ((addr[0]>>4) == 0x0E) Serial.println(" Type: unknown family dallas. May be cyfral in dallas key.");
  else Serial.println(" Type: unknown family dallas");
  keyType = keyUnknown;
  return true;
}

/**/
emRWType getRWtype(){    
  byte answer;
  // TM01 это неизвестный тип болванки, делается попытка записи TM-01 без финализации для dallas или c финализацией под cyfral или metacom
  // RW1990_1 - dallas-совместимые RW-1990, RW-1990.1, ТМ-08, ТМ-08v2 
  // RW1990_2 - dallas-совместимая RW-1990.2
  // TM2004 - dallas-совместимая TM2004 в доп. памятью 1кб
  
  // пробуем определить RW-1990.1
  ibutton.reset(); ibutton.write(0xD1); // проуем снять флаг записи для RW-1990.1  
  ibutton.write_bit(1);                 // записываем значение флага записи = 1 - отключаем запись
  delay(10); pinMode(iButtonPin, INPUT);  
  ibutton.reset(); ibutton.write(0xB5); // send 0xB5 - запрос на чтение флага записи  
  answer = ibutton.read();  
  if (answer == 0xFE){
    Serial.println(" Type: dallas RW-1990.1 ");
    return RW1990_1;
  }

  // пробуем определить RW-1990.2
  ibutton.reset(); ibutton.write(0x1D);  // пробуем установить флаг записи для RW-1990.2   
  ibutton.write_bit(1);                  // записываем значение флага записи = 1 - включаем запись
  delay(10); pinMode(iButtonPin, INPUT);
  ibutton.reset(); ibutton.write(0x1E);  // send 0x1E - запрос на чтение флага записи
  answer = ibutton.read();
  if (answer == 0xFE){
    ibutton.reset(); ibutton.write(0x1D); // возвращаем оратно запрет записи для RW-1990.2
    ibutton.write_bit(0);                 // записываем значение флага записи = 0 - выключаем запись
    delay(10); pinMode(iButtonPin, INPUT);
    Serial.println(" Type: dallas RW-1990.2 ");
    return RW1990_2;
  }

  // пробуем определить TM-2004
  ibutton.reset(); ibutton.write(0x33);                     // посылаем команду чтения ROM для перевода в расширенный 3-х байтовый режим
  for ( byte i = 0; i < 8; i++) ibutton.read();             // читаем данные ключа
  ibutton.write(0xAA);                                      // пробуем прочитать регистр статуса для TM-2004    
  ibutton.write(0x00); ibutton.write(0x00);                 // передаем адрес для считывания
  answer = ibutton.read();                                  // читаем CRC комманды и адреса
  byte m1[3] = {0xAA, 0x00, 0x00};                          // вычисляем CRC комманды
  if (OneWire::crc8(m1, 3) == answer) {
    answer = ibutton.read();                                // читаем регистр статуса    
    Serial.println(" Type: dallas TM2004");
    ibutton.reset();
    return TM2004;
  }
  else{
    sprintf(printBuffer, "// AA0000 : %02X", answer);
    Serial.println(printBuffer);
  }

  ibutton.reset();
  Serial.println(" Type: dallas unknown, trying TM-01!");
  return TM01;// DS1990 unknown type
}

/* Write to TM2004 */
bool write2iBtnTM2004(){
  byte answer; bool result = true;
  ibutton.reset(); ibutton.write(0x3C);                     // команда записи ROM для TM-2004    
  ibutton.write(0x00); ibutton.write(0x00);                 // передаем адрес с которого начинается запись
  //byte tmp[] = {0x03, 0x00, 0x00};
  //ibutton.write_bytes(tmp, 3);
  for (byte i = 0; i < 8; i++){
    digitalWrite(R_Led, !digitalRead(R_Led));
    ibutton.write(keyID[i]);
    answer = ibutton.read();
    //if (OneWire::crc8(m1, 3) != answer){result = false; break;}     // crc не верный
    delayMicroseconds(600); ibutton.write_bit(1); delay(50);         // испульс записи
    pinMode(iButtonPin, INPUT);
    Serial.print('*');
    Sd_WriteStep();
    if (keyID[i] != ibutton.read()) { result = false; break; }    //читаем записанный байт и сравниваем, с тем что должно записаться
  } 
  if (!result){
    ibutton.reset();
    Serial.println(" The key copy faild");
    Sd_ErrorBeep();
    digitalWrite(R_Led, HIGH);
    return false;    
  }
  ibutton.reset();
  Serial.println(" The key has copied successesfully");
  Sd_ReadOK();
  delay(500);
  digitalWrite(R_Led, HIGH);
  return true;
}

/* Write to RW1990.1, RW1990.2, TM-01C(F) */
bool write2iBtnRW1990_1_2_TM01(emRWType rwType){
  byte rwCmd, rwFlag = 1;
  switch (rwType){
    case TM01: rwCmd = 0xC1; break;                  // TM-01C(F)
    case RW1990_1: rwCmd = 0xD1; rwFlag = 0; break;  // RW1990.1  флаг записи инвертирован
    case RW1990_2: rwCmd = 0x1D; break;              // RW1990.2
    default: Serial.println(F("unexpected RW Type")); return false;
  }

  //
  ibutton.reset(); ibutton.write(rwCmd);       // send 'write_flag' command (0xC1 or 0xD1 or 0x1D)
  ibutton.write_bit(rwFlag);                   // write 'allow_writing' flag (0b or 1b)
  delay(10); pinMode(iButtonPin, INPUT);
  ibutton.reset(); ibutton.write(0xD5);        // send 'start_write' command ('0xD5')
  for (byte i = 0; i < 8; i++){
    digitalWrite(R_Led, !digitalRead(R_Led));
    if (rwType == RW1990_1) BurnByte(~keyID[i]);// запись происходит инверсно для RW1990.1
    else BurnByte(keyID[i]);//{ibutton.write(keyID[i]); pinMode(iButtonPin, INPUT);}
    Serial.print('*');
    Sd_WriteStep();
  } 

  ibutton.write(rwCmd);                     // send 'write_flag' command (0xC1 or 0xD1 or 0x1D)
  ibutton.write_bit(!rwFlag);               // write 'disallow writing' flag (0b or 1b)
  delay(10); pinMode(iButtonPin, INPUT);
  digitalWrite(R_Led, LOW);       
  
  //checking is data written 
  if (!dataIsBurningOK()){
    Serial.println(" The key copy faild");
    Sd_ErrorBeep();
    digitalWrite(R_Led, HIGH);
    return false;
  }

  Serial.println(" The key has copied successesfully");

  //finalize Metacom or Cyfral
  if ((keyType == keyMetacom)||(keyType == keyCyfral)){
    ibutton.reset();
    if (keyType == keyCyfral) ibutton.write(0xCA);       // send 0xCA - флаг финализации Cyfral
    else ibutton.write(0xCB);                            // send 0xCB - флаг финализации Metacom
    ibutton.write_bit(1);                                // записываем значение флага финализации = 1 - перевезти формат
    delay(10); pinMode(iButtonPin, INPUT);
  }
  Sd_ReadOK();
  delay(500);
  digitalWrite(R_Led, HIGH);
  return true;
}

void BurnByte(byte data){
  for(byte i = 0; i < 8; i++){ 
    ibutton.write_bit(data & 1);  
    delay(5);
    data = data >> 1;
  }
  pinMode(iButtonPin, INPUT);
}

/* Read the key and compare with 'keyID' */
bool dataIsBurningOK(){
  if (!ibutton.reset()) return false;
  
  byte buff[8];
  ibutton.write(0x33); //read data command
  ibutton.read_bytes(buff, 8);
  
  for (byte i = 0; i < 8; i++)
    if (keyID[i] != buff[i])
      return false;
  return true;
}

/* Write the key('keyID') to iButton */
bool write2iBtn(){  
  if (!ibutton.search(addr)) { 
    ibutton.reset_search(); 
    return false;
  }
  
  Serial.print("The new key code is: ");
  int Check = 0;
  for (byte i = 0; i < 8; i++) {
    Serial.print(addr[i], HEX); Serial.print(" ");  
    if (keyID[i] == addr[i]) Check++;
  }
  if (Check == 8) {
    digitalWrite(R_Led, LOW); 
    Serial.println(" it is the same key. Writing in not needed.");
    Sd_ErrorBeep();
    digitalWrite(R_Led, HIGH);
    delay(500);
    return false;
  }
  emRWType rwType = getRWtype(); // определяем тип RW-1990.1 или 1990.2 или TM-01
  Serial.print("\n Burning iButton ID: ");
  if (rwType == TM2004) return write2iBtnTM2004();
  else return write2iBtnRW1990_1_2_TM01(rwType);
}

/************************** Cyfral (125 kHz) ************************/

/**/
unsigned long pulseAComp(bool pulse, unsigned long timeOut/* = 20000*/){  // pulse HIGH or LOW
  bool AcompState;
  unsigned long tStart = micros();
  do {
    AcompState = (ACSR >> ACO)&1;  // читаем флаг компаратора
    if (AcompState == pulse) {
      tStart = micros();
      do {
        AcompState = (ACSR >> ACO)&1;  // читаем флаг компаратора
        if (AcompState != pulse) return (long)(micros() - tStart);  
      } while ((long)(micros() - tStart) < timeOut);
      return 0;                                                 //таймаут, импульс не вернуся оратно
    }             // end if
  } while ((long)(micros() - tStart) < timeOut);
  return 0;
}

/**/
void ACsetOn(){
  ACSR |= 1<<ACBG;                            // Подключаем ко входу Ain0 1.1V для Cyfral/Metacom
  ADCSRA &= ~(1<<ADEN);                       // выключаем ADC
  ADMUX = (ADMUX&0b11110000) | 0b0011;        // подключаем к AC Линию A3
  ADCSRB |= 1<<ACME;                          // включаем мультиплексор AC
}

/**/
bool read_cyfral(byte* buf, byte CyfralPin){
  unsigned long ti; byte j = 0;
  digitalWrite(CyfralPin, LOW); pinMode(CyfralPin, OUTPUT);  //отклчаем питание от ключа
  delay(100);
  pinMode(CyfralPin, INPUT);  // включаем пиание Cyfral
  ACsetOn(); 
  for (byte i = 0; i < 36; i++){    // чиаем 36 bit
    ti = pulseAComp(HIGH);
    if ((ti == 0) || (ti > 200)) break;                      // not Cyfral
    //if ((ti > 20)&&(ti < 50)) bitClear(buf[i >> 3], 7-j);
    if ((ti > 90) && (ti < 200)) bitSet(buf[i >> 3], 7-j);
    j++; if (j>7) j=0; 
  }
  if (ti == 0) return false;
  if ((buf[0] >> 4) != 0b1110) return false;   /// not Cyfral
  byte test;
  for (byte i = 1; i<4; i++){
    test = buf[i] >> 4;
    if ((test != 1)&&(test != 2)&&(test != 4)&&(test != 8)) return false;
    test = buf[i] & 0x0F;
    if ((test != 1)&&(test != 2)&&(test != 4)&&(test != 8)) return false;
  }
  return true;
}

/**/
bool searchCyfral(){
  for (byte i = 0; i < 8; i++) addr[i] = 0;
  if (!read_cyfral(addr, iButtonPin)) return false; 
  keyType = keyCyfral;
  for (byte i = 0; i < 8; i++) {
    Serial.print(addr[i], HEX); Serial.print(":");
    keyID[i] = addr[i];                               // копируем прочтенный код в ReadID
  }
  Serial.println(" Type: Cyfral ");
  return true;  
}

/************************** EM-Marine (125k Hz) ****************************/

/**/
bool vertEvenCheck(byte* buf){        // проверка четности столбцов с данными
  byte k;
  k = (1&buf[1]>>6) + (1&buf[1]>>1) + 1&buf[2]>>4 + 1&buf[3]>>7 + 1&buf[3]>>2 + 1&buf[4]>>5 + 1&buf[4] + 1&buf[5]>>3 + 1&buf[6]>>6 + 1&buf[6]>>1 + 1&buf[7]>>4;
  if (k&1) return false;
  k = 1&buf[1]>>5 + 1&buf[1] + 1&buf[2]>>3 + 1&buf[3]>>6 + 1&buf[3]>>1 + 1&buf[4]>>4 + 1&buf[5]>>7 + 1&buf[5]>>2 + 1&buf[6]>>5 + 1&buf[6] + 1&buf[7]>>3;
  if (k&1) return false;
  k = 1&buf[1]>>4 + 1&buf[2]>>7 + 1&buf[2]>>2 + 1&buf[3]>>5 + 1&buf[3] + 1&buf[4]>>3 + 1&buf[5]>>6 + 1&buf[5]>>1 + 1&buf[6]>>4 + 1&buf[7]>>7 + 1&buf[7]>>2;
  if (k&1) return false;
  k = 1&buf[1]>>3 + 1&buf[2]>>6 + 1&buf[2]>>1 + 1&buf[3]>>4 + 1&buf[4]>>7 + 1&buf[4]>>2 + 1&buf[5]>>5 + 1&buf[5] + 1&buf[6]>>3 + 1&buf[7]>>6 + 1&buf[7]>>1;
  if (k&1) return false;
  if (1&buf[7]) return false;
  //номер ключа, который написан на корпусе
  rfidData[0] = (0b01111000&buf[1])<<1 | (0b11&buf[1])<<2 | buf[2]>>6;
  rfidData[1] = (0b00011110&buf[2])<<3 | buf[3]>>4;
  rfidData[2] = buf[3]<<5 | (0b10000000&buf[4])>>3 | (0b00111100&buf[4])>>2;
  rfidData[3] = buf[4]<<7 | (0b11100000&buf[5])>>1 | (0b1111&buf[5]);
  rfidData[4] = (0b01111000&buf[6])<<1 | (0b11&buf[6])<<2 | buf[7]>>6;
  return true;
}

/**/
byte ttAComp(unsigned long timeOut/* = 10000*/){  // pulse 0 or 1 or -1 if timeout
  byte AcompState, AcompInitState;
  unsigned long tStart = micros();
  AcompInitState = (ACSR >> ACO)&1;               // читаем флаг компаратора
  do {
    AcompState = (ACSR >> ACO)&1;                 // читаем флаг компаратора
    if (AcompState != AcompInitState) {
      delayMicroseconds(1000/(rfidBitRate*4));    // 1/4 Period on 2 kBps = 125 mks 
      AcompState = (ACSR >> ACO)&1;               // читаем флаг компаратора      
      delayMicroseconds(1000/(rfidBitRate*2));    // 1/2 Period on 2 kBps = 250 mks 
      return AcompState;  
    }
  } while ((long)(micros() - tStart) < timeOut);
  return 2;                                             //таймаут, компаратор не сменил состояние
}

/**/
bool readEM_Marie(byte* buf){
  unsigned long tStart = millis();
  byte ti; byte j = 0, k=0;
  for (int i = 0; i<64; i++){    // читаем 64 bit
    ti = ttAComp();
    if (ti == 2)  break;         //timeout
    if ( ( ti == 0 ) && ( i < 9)) {  // если не находим 9 стартовых единиц - начинаем сначала
      if ((long)(millis()-tStart) > 50) { ti=2; break;}  //timeout
      i = -1; j=0; continue;
    }
    if ((i > 8) && (i < 59)){     //начиная с 9-го бита проверяем контроль четности каждой строки
      if (ti) k++;                // считаем кол-во единиц
      if ( (i-9)%5 == 4 ){        // конец строки с данными из 5-и бит, 
        if (k & 1) {              //если нечетно - начинаем сначала
          i = -1; j = 0; k = 0; continue; 
        }
        k = 0;
      }
    }
    if (ti) bitSet(buf[i >> 3], 7-j);
      else bitClear(buf[i >> 3], 7-j);
    j++; if (j>7) j=0; 
  }
  if (ti == 2) return false;         //timeout
  return vertEvenCheck(buf);
}

/**/
void rfidACsetOn(){
  //включаем генератор 125кГц
  TCCR2A = _BV(COM2A0) | _BV(COM2B1) | _BV(WGM21) | _BV(WGM20);  //Вкючаем режим Toggle on Compare Match на COM2A (pin 11) и счет таймера2 до OCR2A
  TCCR2B = _BV(WGM22) | _BV(CS20);                                // Задаем делитель для таймера2 = 1 (16 мГц)
  OCR2A = 63;                                                    // 63 тактов на период. Частота на COM2A (pin 11) 16000/64/2 = 125 кГц, Скважнось COM2A в этом режиме всегда 50% 
  OCR2B = 31;                                                     // Скважность COM2B 32/64 = 50%  Частота на COM2A (pin 3) 16000/64 = 250 кГц
  // включаем компаратор
  ADCSRB &= ~(1<<ACME);           // отключаем мультиплексор AC
  ACSR &= ~(1<<ACBG);             // отключаем от входа Ain0 1.1V
  digitalWrite(ACpin, LOW); pinMode(ACpin, OUTPUT);                                            // ускоряем переходные процессы в детекторе с 12мс до 2 мс
  delay(1);
  pinMode(ACpin, INPUT);
  delay(1);
}

/*125 kHz RFID*/
bool searchEM_Marine(bool copyKey = true){
  byte gr = digitalRead(G_Led);
  bool rez = false;
  rfidACsetOn();            // включаем генератор 125кГц и компаратор
  delay(4);                //4 мс запускается ключ
  //if (!readEM_Marie(addr)) goto l2;
  if (readEM_Marie(addr)){
    rez = true;
    keyType = keyEM_Marine;
    for (byte i = 0; i<8; i++){
      if (copyKey) keyID[i] = addr [i];
      Serial.print(addr[i], HEX); Serial.print(":");
    }
    Serial.print(" ( id ");
    Serial.print(rfidData[0]); Serial.print(" key ");
    unsigned long keyNum = (unsigned long)rfidData[1]<<24 | (unsigned long)rfidData[2]<<16 | (unsigned long)rfidData[3]<<8 | (unsigned long)rfidData[4];
    Serial.print(keyNum);
    Serial.println(") Type: EM-Marine ");    
  }
  //l2:
  if (!CheckL)
    if (!copyKey) TCCR2A &=0b00111111;              //Оключить ШИМ COM2A (pin 11). Для настройки катушки в резонанс установите CheckL в 1
  digitalWrite(G_Led, gr);
  return rez;
}

/**/
void TxBitRfid(byte data){
  if (data & 1) delayMicroseconds(54*8); 
    else delayMicroseconds(24*8);
  rfidGap(19*8);                       //write gap
}

/**/
void TxByteRfid(byte data){
  for(byte n_bit=0; n_bit<8; n_bit++){
    TxBitRfid(data & 1);
    data = data >> 1;                   // переходим к следующему bit
  }
}

/**/
void rfidGap(unsigned int tm){
  TCCR2A &=0b00111111;                //Оключить ШИМ COM2A 
  delayMicroseconds(tm);
  TCCR2A |= _BV(COM2A0);              // Включить ШИМ COM2A (pin 11)      
}

/**/
bool T5557_blockRead(byte* buf){
  byte ti; byte j = 0, k=0;
  for (int i = 0; i<33; i++){    // читаем стартовый 0 и 32 значащих bit
    ti = ttAComp(2000);
    if (ti == 2)  break;         //timeout
    if ( ( ti == 1 ) && ( i == 0)) { ti=2; break; }                             // если не находим стартовый 0 - это ошибка
    if (i > 0){     //начиная с 1-го бита пишем в буфер
      if (ti) bitSet(buf[(i-1) >> 3], 7-j);
        else bitClear(buf[(i-1) >> 3], 7-j);
      j++; if (j>7) j=0;
    }
  }
  if (ti == 2) return false;         //timeout
  return true;
}

/**/
bool sendOpT5557(byte opCode, unsigned long password = 0, byte lockBit = 0, unsigned long data = 0, byte blokAddr = 1){
  TxBitRfid(opCode >> 1); TxBitRfid(opCode & 1); // передаем код операции 10
  if (opCode == 0b00) return true;
  // password
  TxBitRfid(lockBit & 1);               // lockbit 0
  if (data != 0){
    for (byte i = 0; i<32; i++) {
      TxBitRfid((data>>(31-i)) & 1);
    }
  }
  TxBitRfid(blokAddr>>2); TxBitRfid(blokAddr>>1); TxBitRfid(blokAddr & 1);      // адрес блока для записи
  delay(4);                       // ждем пока пишутся данные
  return true;
}

/**/
bool write2rfidT5557(byte* buf){
  bool result; unsigned long data32;
  digitalWrite(R_Led, LOW);
  delay(6);
  for (byte k = 0; k<2; k++){                                       // send key data
    data32 = (unsigned long)buf[0 + (k<<2)]<<24 | (unsigned long)buf[1 + (k<<2)]<<16 | (unsigned long)buf[2 + (k<<2)]<<8 | (unsigned long)buf[3 + (k<<2)];
    rfidGap(30 * 8);                                                 //start gap
    sendOpT5557(0b10, 0, 0, data32, k+1);                            //передаем 32 бита ключа в blok k
    Serial.print('*'); delay(6);
  }
  delay(6);
  rfidGap(30 * 8);          //start gap
  sendOpT5557(0b00);
  result = readEM_Marie(addr);
  TCCR2A &=0b00111111;              //Оключить ШИМ COM2A (pin 11)
  for (byte i = 0; i < 8; i++)
    if (addr[i] != keyID[i]) { result = false; break; }
  if (!result){
    Serial.println(" The key copy faild");
    Sd_ErrorBeep();
  } else {
    Serial.println(" The key has copied successesfully");
    Sd_ReadOK();
    delay(1000);
  }
  digitalWrite(R_Led, HIGH);
  return result;  
}

/**/
emRWType getRfidRWtype(){
  unsigned long data32, data33; byte buf[4] = {0, 0, 0, 0}; 
  rfidACsetOn();            // включаем генератор 125кГц и компаратор
  delay(4);                //4мс запускается ключ
  rfidGap(30 * 8);          //start gap
  sendOpT5557(0b11, 0, 0, 0, 1); //переходим в режим чтения Vendor ID 
  if (!T5557_blockRead(buf)) return rwUnknown; 
  data32 = (unsigned long)buf[0]<<24 | (unsigned long)buf[1]<<16 | (unsigned long)buf[2]<<8 | (unsigned long)buf[3];
  delay(4);
  rfidGap(20 * 8);          //gap  
  data33 = 0b00000000000101001000000001000000 | (rfidUsePWD << 4);   //конфиг регистр 0b00000000000101001000000001000000
  sendOpT5557(0b10, 0, 0, data33, 0);   //передаем конфиг регистр
  delay(4);
  rfidGap(30 * 8);          //start gap
  sendOpT5557(0b11, 0, 0, 0, 1); //переходим в режим чтения Vendor ID 
  if (!T5557_blockRead(buf)) return rwUnknown; 
  data33 = (unsigned long)buf[0]<<24 | (unsigned long)buf[1]<<16 | (unsigned long)buf[2]<<8 | (unsigned long)buf[3];
  sendOpT5557(0b00, 0, 0, 0, 0);  // send Reset
  delay(6);
  if (data32 != data33) return rwUnknown;    
  Serial.print(" The rfid RW-key is T5557. Vendor ID is ");
  Serial.println(data32, HEX);
  return T5557;
}

/**/
bool write2rfid(){
  bool Check = true;
  if (searchEM_Marine(false)) {
    for (byte i = 0; i < 8; i++)
      if (addr[i] != keyID[i]) { Check = false; break; }  // сравниваем код для записи с тем, что уже записано в ключе.
    if (Check) {                                          // если коды совпадают, ничего писать не нужно
      digitalWrite(R_Led, LOW); 
      Serial.println(" it is the same key. Writing in not needed.");
      Sd_ErrorBeep();
      digitalWrite(R_Led, HIGH);
      delay(500);
      return false;
    }
  }
  emRWType rwType = getRfidRWtype(); // определяем тип T5557 (T5577) или EM4305
  if (rwType != rwUnknown) Serial.print("\n Burning rfid ID: ");
  //keyID[0] = 0xFF; keyID[1] = 0x8E; keyID[2] =  0xE0; keyID[3] = 0x2; keyID[4] = 0x9E; keyID[5] = 0x1; keyID[6] = 0x96; keyID[7] = 0x82; //keyID[0] = 0xFF; keyID[1] = 0x8E; keyID[2] =  0xE0; keyID[3] = 0x2; keyID[4] = 0x9E; keyID[5] = 0x1; keyID[6] = 0x96; keyID[7] = 0x82;  // (Мой код-пошел на Т5557)если у вас есть код какого-то ключа, можно прописать его тут
  switch (rwType){
    case T5557: return write2rfidT5557(keyID); break;                    //пишем T5557
    //case EM4305: return write2rfidEM4305(keyID); break;                  //пишем EM4305
    case rwUnknown: break;
  }
  return false;
}

/************************** SOUND **************************/

void Sd_ReadOK(){  
  for (int i=400; i<6000; i=i*1.5) { tone(speakerPin, i); delay(20); }
  noTone(speakerPin);
}

void Sd_WriteStep(){  
  for (int i=2500; i<6000; i=i*1.5) { tone(speakerPin, i); delay(10); }
  noTone(speakerPin);
}

void Sd_ErrorBeep(){  
  for (int j=0; j <3; j++){
    for (int i=1000; i<2000; i=i*1.1) { tone(speakerPin, i); delay(50); }
    delay(50);
    for (int i=1000; i>500; i=i*1.9) { tone(speakerPin, i); delay(10); }
    delay(50);
  }
  noTone(speakerPin);
}

void Sd_StartOK(){   
  tone(speakerPin, NOTE_A7); delay(100);
  /*tone(speakerPin, NOTE_G7); delay(100);
  tone(speakerPin, NOTE_E7); delay(100); 
  tone(speakerPin, NOTE_C7); delay(100);  
  tone(speakerPin, NOTE_D7); delay(100); 
  tone(speakerPin, NOTE_B7); delay(100); 
  tone(speakerPin, NOTE_F7); delay(100); 
  tone(speakerPin, NOTE_C7); delay(100);*/
  noTone(speakerPin); 
}
