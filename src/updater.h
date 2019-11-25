/*
 * updater.h
 *
 *  Created on: 2019-08-18 19:00
 *      Author: Jack Chen <redchenjs@live.com>
 */

#ifndef UPDATER_H
#define UPDATER_H

#include <QtCore>
#include <QtSerialPort/QSerialPort>

class updater_class : public QObject
{
    Q_OBJECT

public:
    int exec(int argc, char *argv[]);

private:
    QSerialPort *m_device = nullptr;
    size_t m_device_rsp = 0;

    bool send_byte(const char c);
    bool send_string(QString *s);

    void process_data(void);

    int open_device(const QString &devname);
    int close_device(void);

    int update_firmware(const QString &devname, QString filename);
    int get_device_info(const QString &devname);
    int reset_device(const QString &devname);

    void print_usage(void);
};

#endif // UPDATER_H