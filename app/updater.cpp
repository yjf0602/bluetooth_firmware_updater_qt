/*
 * updater.cpp
 *
 *  Created on: 2019-08-18 19:00
 *      Author: Jack Chen <redchenjs@live.com>
 */

#include <string>
#include <iostream>
#include <QtCore>
#include <QThread>
#include <QtSerialPort/QSerialPort>
#include "updater.h"

bool updater_class::send_byte(const char c)
{
    if (!m_device->isOpen()) {
        return false;
    }

    if ((m_device->write(&c, 1)) < 1) {
        return false;
    }

    return true;
}

bool updater_class::send_string(QString *s)
{
    QByteArray bytes;
    bytes.reserve(s->size());
    for (auto &c : *s) {
        bytes.append(static_cast<char>(c.unicode()));
    }

    for (int i=0; i<bytes.length(); i++) {
        if (!send_byte(bytes[i])) {
            return false;
        }
    }

    if (!send_byte('\r') || !send_byte('\n')) {
        return false;
    }

    return true;
}

void updater_class::process_data(void)
{
    QString data = m_device->readAll();

    if (data.contains("VER")) {
        if (data.contains("\r\n")) {
            std::cout << "<< " << data.toStdString();
        } else {
            std::cout << "<< " << data.toStdString() << std::endl;
        }
        m_device_rsp = 1;
    } else if (data.contains("OK")) {
        std::cout << "<< OK" << std::endl;
        m_device_rsp = 1;
    } else if (data.contains("DONE")) {
        std::cout << "<< DONE" << std::endl;
        m_device_rsp = 1;
    } else if (data.contains("ERROR")) {
        std::cout << "<< ERROR" << std::endl;
        m_device_rsp = 2;
    } else if (data.contains("LOCKED")) {
        std::cout << "<< LOCKED" << std::endl;
        m_device_rsp = 2;
    }
}

int updater_class::exec(int argc, char *argv[])
{
    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " /dev/rfcommX firmware.bin" << std::endl;
        return -1;
    }

    m_device = new QSerialPort(this);
    connect(m_device, &QSerialPort::readyRead, this, &updater_class::process_data);

    // open serial device
    m_device->setPortName(argv[1]);
    if (m_device->open(QIODevice::ReadWrite)) {
        m_device->setBaudRate(115200);
        m_device->setDataBits(QSerialPort::Data8);
        m_device->setParity(QSerialPort::NoParity);
        m_device->setStopBits(QSerialPort::OneStop);
        m_device->setFlowControl(QSerialPort::NoFlowControl);
        m_device->flush();
    } else {
        std::cout << "could not open device" << std::endl;
        return -2;
    }

    // open firmware file
    QString filename = QString(argv[2]);
    QFile fd(filename);
    if (!fd.open(QIODevice::ReadOnly)) {
        std::cout << "could not open file" << std::endl;
        return -3;
    }

    // get target firmware version
    QString cmd = QString("FW+VER?");
    std::cout << ">> " << cmd.toStdString() << std::endl;
    send_string(&cmd);
    m_device->waitForBytesWritten();
    while (m_device_rsp == 0) {
        QThread::msleep(50);
        if (m_device_rsp == 0) {
            m_device->waitForReadyRead();
        }
    }
    m_device_rsp = 0;

    // send update command to target device
    qint64 filesize = fd.size();
    cmd = QString("FW+UPD:") + cmd.number(filesize);
    std::cout << ">> " << cmd.toStdString() << std::endl;
    send_string(&cmd);
    m_device->waitForBytesWritten();
    while (m_device_rsp == 0) {
        QThread::msleep(50);
        if (m_device_rsp == 0) {
            m_device->waitForReadyRead();
        }
    }
    // error
    if (m_device_rsp == 2) {
        return -4;
    }
    m_device_rsp = 0;

    // send firmware data
    QByteArray filedata = fd.readAll();
    for (int i=0; i<filedata.size(); i++) {
        bool rc = send_byte(filedata.at(i));
        if (!rc) {
            std::cout << "write failed" << std::endl;
            return -5;
        }
        // flush every 32k data
        if ((i+1) % 32768 == 0) {
            m_device->waitForBytesWritten();
            QThread::msleep(750);
        }
        std::cout << ">> SENT:" << i*100/filedata.size() << "%\r";
    }
    std::cout << std::endl;
    m_device->waitForBytesWritten();
    while (m_device_rsp == 0) {
        QThread::msleep(50);
        if (m_device_rsp == 0) {
            m_device->waitForReadyRead();
        }
    }
    // error
    if (m_device_rsp == 2) {
        return -6;
    }
    m_device_rsp = 0;

    // reset target device
    cmd = QString("FW+RST");
    std::cout << ">> " << cmd.toStdString() << std::endl;
    send_string(&cmd);
    m_device->waitForBytesWritten();

    // close firmware file
    fd.close();

    // close serial device
    m_device->clearError();
    m_device->close();

    return 0;
}