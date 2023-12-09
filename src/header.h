#define DEBUG
/* DS1992 pinout:
RED:    GND
GREEN:  DATA (I/O) connect via 5KOm resistor
BLACK:  LED VCC (3V)
WHITE:  LED GND
*/
/* DS ID: 8 bits family code + 48 bits serial number + 8 bits CRC */

#define CheckL 0           // для настройки катушки временно установите тут единицу
#define iButtonPin 10      // Линия data ibutton
#define R_Led 2            // RGB Led
#define G_Led 3
#define B_Led 4
//#define VRpinGnd 5         // Земля подстроечного резистора для аналогового компаратора
#define ACpin 6            // Вход Ain0 аналогового компаратора для EM-Marine
#define BtnPin 26           // Кнопка переключения режима чтение/запись
//#define BtnPinGnd 9        // Земля кнопки переключения режима 
#define speakerPin 27/*10*/       // Спикер, он же buzzer, он же beeper
#define FreqGen 11         // генератор 125 кГц
//#define speakerPinGnd 12   // земля buzzer
#define rfidBitRate 2       // Скорость обмена с rfid в kbps
#define rfidUsePWD 0        // ключ использует пароль для изменения
#define rfidPWD 123456      // пароль для ключа

OneWire ibutton (iButtonPin); 
byte addr[8];                             // временный буфер
byte keyID[8];                            // ID ключа для записи
byte rfidData[5];                         // значащие данные frid em-marine
bool readflag = false;                    // флаг сигнализирует, что данные с ключа успечно прочианы в ардуино
bool writeflag = false;                   // режим запись/чтение
bool preBtnPinSt = HIGH;
enum emRWType {rwUnknown, TM01, RW1990_1, RW1990_2, TM2004, T5557, EM4305};   // тип болванки
enum emkeyType {keyUnknown, keyDS1990A, keyTM2004, keyCyfral, keyMetacom, keyEM_Marine};    // тип оригинального ключа  
emkeyType keyType; //original key data type
char printBuffer[128];

int writeByte(byte);
void writeBit(bool);

void clearLed(void);

/************************** DALLAS **************************/
bool searchIbutton(void);
emRWType getRWtype(void);
bool write2iBtn(void);
bool write2iBtnRW1990_1_2_TM01(emRWType rwType);
bool write2iBtnTM2004(void);
bool dataIsBurningOK(void);
void BurnByte(byte data);

/************************** Cyfral ************************/
unsigned long pulseAComp(bool pulse, unsigned long timeOut = 20000);
void ACsetOn(void);
bool read_cyfral(byte* buf, byte CyfralPin);
bool searchCyfral(void);

/************************** EM-Marine ***************************/
bool vertEvenCheck(byte* buf);
byte ttAComp(unsigned long timeOut = 10000);
bool readEM_Marie(byte* buf);
void rfidACsetOn(void);
bool searchEM_Marine(bool copyKey = true);
void TxBitRfid(byte data);
void TxByteRfid(byte data);
void rfidGap(unsigned int tm);
bool T5557_blockRead(byte* buf);
bool sendOpT5557(byte opCode, unsigned long password = 0, byte lockBit = 0, unsigned long data = 0, byte blokAddr = 1);
bool write2rfidT5557(byte* buf);
emRWType getRfidRWtype(void);
bool write2rfid(void);

/************************** Sound ****************************/
void Sd_ReadOK(void);
void Sd_WriteStep(void);
void Sd_ErrorBeep(void);
void Sd_StartOK(void);
