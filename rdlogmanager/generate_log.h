// generate_log.h
//
// Generate a Rivendell Log
//
//   (C) Copyright 2002-2021 Fred Gleason <fredg@paravelsystems.com>
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License version 2 as
//   published by the Free Software Foundation.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public
//   License along with this program; if not, write to the Free Software
//   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//

#ifndef GENERATE_LOG_H
#define GENERATE_LOG_H

#include <QCalendarWidget>
#include <QCheckBox>
#include <QMap>
#include <QProgressDialog>
#include <QSet>
#include <QTextCharFormat>

#include <rdcombobox.h>
#include <rddialog.h>
#include <rdnotification.h>

#define GENERATE_LOG_FILESCAN_INTERVAL 5000


class MultiDateCalendar : public QCalendarWidget
{
 Q_OBJECT
 public:
  MultiDateCalendar(QWidget *parent=0);
  QSet<QDate> markedDates() const;
  int markedCount() const;
  void clearMarked();
  void setLogStatuses(const QMap<QDate,int> &statuses);

 signals:
  void markedDatesChanged(int count);

 protected:
  void paintCell(QPainter *painter,const QRect &rect,const QDate &date) const;

 private slots:
  void dateClickedSlot(const QDate &date);

 private:
  void refreshFormats();
  QSet<QDate> cal_dates;          // dates marked for generation (shown blue)
  QMap<QDate,int> cal_log_status; // 1=generated (yellow), 2=traffic merged (green)
  QSet<QDate> cal_formatted;      // every date we have called setDateTextFormat on
};


class GenerateLog : public RDDialog
{
 Q_OBJECT
 public:
  GenerateLog(QWidget *parent=0,int cmd_switch=0,QString *cmd_service=NULL,QDate *cmd_date=NULL);
  QSize sizeHint() const;
  QSizePolicy sizePolicy() const;

 private slots:
  void serviceActivatedData(int index);
  void markedDatesChangedData(int count);
  void clearMarkedData();
  void selectionChangedData();
  void pageChangedData(int year,int month);
  void createData();
  void musicData();
  void trafficData();
  void fileScanData();
  void closeData();

 protected:
  void resizeEvent(QResizeEvent *e);

 private:
  void UpdateControls();
  void updateCalendarHighlights();
  void SendNotification(RDNotification::Action action,const QString &logname);
  QLabel *gen_service_label;
  QComboBox *gen_service_box;
  MultiDateCalendar *gen_calendar;
  QLabel *gen_count_label;
  QPushButton *gen_clear_button;
  QCheckBox *gen_replace_check;
  QCheckBox *gen_merge_traffic_check;
  QLabel *gen_import_label;
  QLabel *gen_available_label;
  QLabel *gen_merged_label;
  QProgressDialog *gen_progress_dialog;
  QPushButton *gen_create_button;
  QPushButton *gen_music_button;
  QPushButton *gen_traffic_button;
  QPushButton *gen_close_button;
  QLabel *gen_tfc_avail_label;
  QLabel *gen_tfc_merged_label;
  QLabel *gen_mus_avail_label;
  QLabel *gen_mus_merged_label;
  bool gen_music_enabled;
  bool gen_traffic_enabled;
  int cmdswitch;
  QString *cmdservice;
  QDate *cmddate;
};


#endif  // GENERATE_LOG_H
