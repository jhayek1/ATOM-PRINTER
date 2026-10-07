#include "ATOM_PRINTER.h"
#include "ATOM_PRINTER_CMD.h"

void ATOM_PRINTER::begin(HardwareSerial *serial, int baud, uint8_t RX, uint8_t TX, bool debug)
{
    _debug  = debug;
    _serial = serial;
    _serial->begin(baud, SERIAL_8N1, RX, TX);
}

void ATOM_PRINTER::init()
{
    _serial->write(INIT_PRINTER_CMD, sizeof(INIT_PRINTER_CMD));
}

void ATOM_PRINTER::WriteCMD(uint8_t *buff, uint8_t buff_size)
{
    _serial->write(buff, buff_size);
}

void ATOM_PRINTER::printPos(uint16_t posx)
{
    _serial->write(PRINT_POS_CMD, sizeof(PRINT_POS_CMD));
    _serial->write(posx & 0xff);
    _serial->write((posx >> 8) & 0xff);
}

void ATOM_PRINTER::fontSize(uint8_t font_size)
{
    if (font_size > 7) font_size = 7;
    _serial->write(FONT_SIZE_CMD, sizeof(FONT_SIZE_CMD));
    _serial->write((font_size | (font_size << 4)) & 0xff);
}

void ATOM_PRINTER::newLine(uint8_t count)
{
    for (uint8_t i = 0; i < count; i++) {
        _serial->write(0x0A);
    }
}

void ATOM_PRINTER::printASCII(String data)
{
    _serial->print(data);
}

void ATOM_PRINTER::printQRCode(String qrcode)
{
    // set qrcode; the length field is 16 bit (pL pH) and counts the 3 bytes after it
    uint16_t len = qrcode.length() + 3;
    uint8_t header[sizeof(SET_QRCODE_CMD)];
    memcpy(header, SET_QRCODE_CMD, sizeof(SET_QRCODE_CMD));
    header[3] = len & 0xff;
    header[4] = (len >> 8) & 0xff;
    _serial->write(header, sizeof(header));
    _serial->print(qrcode);
    _serial->write(0x00);
    // print qrcode
    _serial->write(PRINTER_QRCODE_CMD, sizeof(PRINTER_QRCODE_CMD));
}

void ATOM_PRINTER::setBarCodeHRI(BarCodePos_t pos)
{
    _serial->write(SET_BAR_CODE_POS_CMD, sizeof(SET_BAR_CODE_POS_CMD));
    _serial->write(pos);
}
void ATOM_PRINTER::setQRCodeECL(QRCode_EC_Level_t level)
{
    _serial->write(SET_QRCODE_ECL_CODE_CMD, sizeof(SET_QRCODE_ECL_CODE_CMD));
    _serial->write(level);
}

void ATOM_PRINTER::enableBarCode(bool state)
{
    _serial->write(ENABLE_BAR_CODE_MODE_CMD, sizeof(ENABLE_BAR_CODE_MODE_CMD) - 1);
    _serial->write(state);
}

void ATOM_PRINTER::printBarCode(BarCode_t type, String barcode)
{
    // The length field is a single byte, so longer data would wrap around
    if (barcode.length() > 255) barcode = barcode.substring(0, 255);
    enableBarCode(1);
    _serial->write(PRINTER_BAR_CODE_CMD, 2);
    _serial->write(type);
    _serial->write((uint8_t)barcode.length());
    _serial->print(barcode);
    _serial->write(0x00);
    enableBarCode(0);
}

void ATOM_PRINTER::printBMP(uint8_t mode, uint16_t xdot, uint16_t ydot, uint8_t *bmpdata)
{
    if (mode > 3) mode = 3;
    uint16_t bytes_per_row = xdot / 8;
    uint8_t header[sizeof(PRINTER_BMP_CMD)];
    memcpy(header, PRINTER_BMP_CMD, sizeof(PRINTER_BMP_CMD));
    header[3] = mode;
    header[4] = (uint8_t)(bytes_per_row & 0x00ff);
    header[5] = (uint8_t)((bytes_per_row >> 8) & 0x00ff);
    header[6] = (uint8_t)(ydot & 0x00ff);
    header[7] = (uint8_t)((ydot >> 8) & 0x00ff);
    _serial->write(header, sizeof(header));
    _serial->write(bmpdata, (size_t)bytes_per_row * ydot);
}

void ATOM_PRINTER::printRaw(const uint8_t *data, size_t size)
{
    _serial->write(data, size);
}
