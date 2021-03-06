/* This file is part of QJson
 *
 * Copyright (C) 2008 Flavio Castelli <flavio.castelli@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "json_scanner.h"
#include "json_parser.h"

#include <ctype.h>

#include <QDebug>
#include <QRegExp>

JSonScanner::JSonScanner(QIODevice* io)
  : m_io (io)
{
  m_quotmarkClosed = true;
  m_quotmarkCount = 0;
}

int JSonScanner::yylex(YYSTYPE* yylval, yy::location *yylloc)
{
  char ch;
  
  if (!m_io->isOpen()) {
    qDebug() << "JSonScanner::yylex - io device is not open";
    return -1;
  }

  yylloc->step();

  do {
    bool ret;
    if (m_io->atEnd()) {
      qDebug() << "JSonScanner::yylex - yy::json_parser::token::END";
      return yy::json_parser::token::END;
    }
    else
      ret = m_io->getChar(&ch);

    if (!ret) {
      qDebug() << "JSonScanner::yylex - error reading from io device";
      return -1;
    }

    qDebug() << "JSonScanner::yylex - got |" << ch << "|";
    
    yylloc->columns();
    
    if (ch == '\n' || ch == '\r')
      yylloc->lines();
      
  } while (m_quotmarkClosed && (isspace(ch) != 0));

  if (m_quotmarkClosed && ((ch == 't') || (ch == 'T')
      || (ch == 'n') || (ch == 'N'))) {
    // check true & null value
    char buf [3];
  
    if (m_io->peek(buf, sizeof(buf)) == sizeof(buf)) {
      QString value (buf);
      value.truncate (3);
      
      if (QString::compare ("rue", value, Qt::CaseInsensitive) == 0) {
        m_io->read (buf, 3);
        yylloc->columns(3);
        qDebug() << "JSonScanner::yylex - TRUE_VAL";
        return yy::json_parser::token::TRUE_VAL;
      }
      else if (QString::compare ("ull", value, Qt::CaseInsensitive) == 0) {
        m_io->read (buf, 3);
        yylloc->columns(3);
        qDebug() << "JSonScanner::yylex - NULL_VAL";
        return yy::json_parser::token::NULL_VAL;
      }
    }
  }
  else if (m_quotmarkClosed && ((ch == 'f') || (ch == 'F'))) {
    // check false value
    char buf [4];
    
    if (m_io->peek(buf, sizeof(buf)) == sizeof(buf)) {
      QString value (buf);
      value.truncate (4);
      
      if (QString::compare ("alse", value, Qt::CaseInsensitive) == 0) {
        m_io->read (buf, 4);
        yylloc->columns(4);
        qDebug() << "JSonScanner::yylex - FALSE_VAL";
        return yy::json_parser::token::FALSE_VAL;
      }
    }
  }
  else if (m_quotmarkClosed && ((ch == 'e') || (ch == 'E'))) {
    char buf [1];
    QString ret (ch);
    if (m_io->peek(buf, sizeof(buf)) == sizeof(buf)) {
      if ((buf[0] == '+' ) || (buf[0] == '-' )) {
        m_io->read (buf, 1);  
        yylloc->columns();
        ret += buf[0];
      }
    }
    *yylval = QVariant(QString(ret));
    return yy::json_parser::token::E;
  }
  
  if ((ch != '"') && (!m_quotmarkClosed)) {
    // we're inside a " " block
    if (ch == '\\') {
      char buf;
      if (m_io->read (&buf, 1) == -1)
      {
        yylloc->columns();
        if (((buf != '"') && (buf != '\\') && (buf != '/') && 
            (buf != 'b') && (buf != 'f') && (buf != 'n') &&
            (buf != 'r') && (buf != 't') && (buf != 'u'))) {
              qDebug() << "Just read" << buf;
              qDebug() << "JSonScanner::yylex - error decoding escaped sequence";
              return -1;
        }
      } else {
        qDebug() << "JSonScanner::yylex - error decoding escaped sequence : io error";
        return -1;
      }
    
      //TODO handle /u
      
      QString sequence ("\\");
      switch (buf) {
        case 'b':
          sequence = '\b';
          break;
        case 'f':
          sequence = '\f';
          break;
        case 'n':
          sequence = '\n';
          break;
        case 'r':
          sequence = '\r';
          break;            
        case 't':
          sequence = '\t';
          break;
        case 'u':
          break;
        case '/':
          sequence = '/';
          break;
        default:
          sequence += buf;
          break;
      }

      *yylval = QVariant(QString(sequence));
      qDebug() << "JSonScanner::yylex - yy::json_parser::token::WORD ("
               << yylval->toString() << ")";
      return yy::json_parser::token::WORD;
    }
    else {
      *yylval = QVariant(QString(ch));
      qDebug() << "JSonScanner::yylex - yy::json_parser::token::WORD ("
             << yylval->toString() << ")";
      return yy::json_parser::token::WORD;
    }
  }
  else if ((isdigit(ch) != 0) && (m_quotmarkClosed)) {
    *yylval = QVariant(QString(ch));
    qDebug() << "JSonScanner::yylex - yy::json_parser::token::DIGIT";
    return yy::json_parser::token::DIGIT;
  }
  else if (isalnum(ch) != 0) {
    *yylval = QVariant(QString(ch));
    qDebug() << "JSonScanner::yylex - yy::json_parser::token::WORD ("
             << ch << ")";
    return yy::json_parser::token::WORD;
  }
  else if (ch == ':') {
    // set yylval
    qDebug() << "JSonScanner::yylex - yy::json_parser::token::COLON";
    return yy::json_parser::token::COLON;
  }
  else if (ch == '"') {
    // yy::json_parser::token::QUOTMARK (")

    // set yylval
    m_quotmarkCount++;
    if (m_quotmarkCount %2 == 0) {
      m_quotmarkClosed = true;
      m_quotmarkCount = 0;
      qDebug() << "JSonScanner::yylex - yy::json_parser::token::QUOTMARKCLOSE";
      return yy::json_parser::token::QUOTMARKCLOSE;
    }
    else {
      m_quotmarkClosed = false;
      qDebug() << "JSonScanner::yylex - yy::json_parser::token::QUOTMARKOPEN";
      return yy::json_parser::token::QUOTMARKOPEN;
    }
  }
  else if (ch == ',') {
    qDebug() << "JSonScanner::yylex - yy::json_parser::token::COMMA";
    return yy::json_parser::token::COMMA;
  }
  else if (ch == '.') {
    qDebug() << "JSonScanner::yylex - yy::json_parser::token::DOT";
    return yy::json_parser::token::DOT;
  }
  else if (ch == '-') {
    qDebug() << "JSonScanner::yylex - yy::json_parser::token::MINUS";
    return yy::json_parser::token::MINUS;
  }
  else if (ch == '[') {
    qDebug() << "JSonScanner::yylex - yy::json_parser::token::SQUARE_BRACKET_OPEN";
    return yy::json_parser::token::SQUARE_BRACKET_OPEN;
  }
  else if (ch == ']') {
    qDebug() << "JSonScanner::yylex - yy::json_parser::token::SQUARE_BRACKET_CLOSE";
    return yy::json_parser::token::SQUARE_BRACKET_CLOSE;
  }
  else if (ch == '{') {
    qDebug() << "JSonScanner::yylex - yy::json_parser::token::CURLY_BRACKET_OPEN";
    return yy::json_parser::token::CURLY_BRACKET_OPEN;
  }
  else if (ch == '}') {
    qDebug() << "JSonScanner::yylex - yy::json_parser::token::CURLY_BRACKET_CLOSE";
    return yy::json_parser::token::CURLY_BRACKET_CLOSE;
  }

  //unknown char!
  //TODO yyerror?
  qDebug() << "JSonScanner::yylex - unknown char, returning -1";
  return -1;
}


