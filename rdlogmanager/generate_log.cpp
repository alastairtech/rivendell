// generate_log.cpp
//
// Generate a Rivendell Log
//
//   (C) Copyright 2002-2022 Fred Gleason <fredg@paravelsystems.com>
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

#include <algorithm>

#include <QCoreApplication>
#include <QFile>
#include <QMessageBox>
#include <QPainter>

#include <rddatedecode.h>
#include <rdescape_string.h>
#include <rdlog.h>
#include <rdsvc.h>
#include <rdtextfile.h>

#include "edit_grid.h"
#include "generate_log.h"
#include "globals.h"


//
// MultiDateCalendar
//

MultiDateCalendar::MultiDateCalendar(QWidget *parent)
  : QCalendarWidget(parent)
{
  // Suppress the native single-date selection cursor so it cannot be
  // confused with our own blue "marked for generation" highlight.
  setStyleSheet("QCalendarWidget QAbstractItemView:enabled {"
		"  selection-background-color: white;"
		"  selection-color: black; }");
  connect(this,SIGNAL(clicked(const QDate &)),
	  this,SLOT(dateClickedSlot(const QDate &)));
}


QSet<QDate> MultiDateCalendar::markedDates() const
{
  return cal_dates;
}


int MultiDateCalendar::markedCount() const
{
  return cal_dates.size();
}


void MultiDateCalendar::clearMarked()
{
  cal_dates.clear();
  refreshFormats();
  emit markedDatesChanged(0);
}


void MultiDateCalendar::setLogStatuses(const QMap<QDate,int> &statuses)
{
  cal_log_status=statuses;
  refreshFormats();
}


void MultiDateCalendar::refreshFormats()
{
  // Clear every format we previously set so no stale colour lingers.
  foreach(const QDate &d,cal_formatted) {
    setDateTextFormat(d,QTextCharFormat());
  }
  cal_formatted.clear();

  // Apply log-status colours for dates not currently marked (blue wins).
  for(QMap<QDate,int>::const_iterator it=cal_log_status.constBegin();
      it!=cal_log_status.constEnd();++it) {
    if(!cal_dates.contains(it.key())) {
      QTextCharFormat fmt;
      if(it.value()==2)
        fmt.setBackground(QColor(144,238,144));   // green: traffic merged
      else
        fmt.setBackground(QColor(255,255,153));   // yellow: generated only
      fmt.setForeground(Qt::black);
      setDateTextFormat(it.key(),fmt);
      cal_formatted.insert(it.key());
    }
  }

  // Apply blue for dates marked for generation.
  foreach(const QDate &d,cal_dates) {
    QTextCharFormat fmt;
    fmt.setBackground(QColor(147,197,253));
    fmt.setForeground(Qt::black);
    setDateTextFormat(d,fmt);
    cal_formatted.insert(d);
  }

  updateCells();
}


void MultiDateCalendar::paintCell(QPainter *painter,const QRect &rect,
				  const QDate &date) const
{
  QCalendarWidget::paintCell(painter,rect,date);
  if(cal_formatted.contains(date)) {
    QTextCharFormat fmt=dateTextFormat(date);
    QColor bg=fmt.background().color();
    if(bg.isValid()) {
      // Overdraw whatever the parent (including selection highlight) painted.
      painter->save();
      painter->fillRect(rect.adjusted(1,1,-1,-1),bg);
      painter->setPen(Qt::black);
      painter->drawText(rect,Qt::AlignCenter,QString::number(date.day()));
      painter->restore();
    }
  }
}


void MultiDateCalendar::dateClickedSlot(const QDate &date)
{
  if(cal_dates.contains(date))
    cal_dates.remove(date);
  else
    cal_dates.insert(date);
  refreshFormats();
  emit markedDatesChanged(cal_dates.size());
}


//
// GenerateLog
//

GenerateLog::GenerateLog(QWidget *parent,int cmd_switch,QString *cmd_service,
			 QDate *cmd_date)
  : RDDialog(parent)
{
  QStringList services_list;
  bool cmdservicefit=false;
  cmdswitch=cmd_switch;
  cmdservice=cmd_service;
  cmddate=cmd_date;

  setWindowTitle("RDLogManager - "+tr("Generate Log"));

  gen_music_enabled=false;
  gen_traffic_enabled=false;

  //
  // Fix the Window Size
  //
  setMinimumSize(sizeHint());
  setMaximumSize(sizeHint());

  gen_progress_dialog=nullptr;

  //
  // Service Name
  //
  gen_service_box=new QComboBox(this);
  connect(gen_service_box,SIGNAL(activated(int)),
	  this,SLOT(serviceActivatedData(int)));
  gen_service_label=new QLabel(tr("Service:"),this);
  gen_service_label->setFont(labelFont());
  gen_service_label->setAlignment(Qt::AlignRight|Qt::AlignVCenter);

  QString sql="select `NAME` from `SERVICES`";
  RDSqlQuery *q=new RDSqlQuery(sql);
  while(q->next()) {
    services_list.append(q->value(0).toString());
  }
  delete q;
  gen_service_box->insertItem(0,tr("[select service]"));
  for(QStringList::Iterator it=services_list.begin();
      it!=services_list.end();
      ++it) {
    gen_service_box->
      insertItem(gen_service_box->count(),rda->iconEngine()->serviceIcon(),*it);
    if(cmdswitch!=0 && *cmdservice==*it)
      cmdservicefit=true;
  }

  //
  // Multi-date Calendar
  //
  gen_calendar=new MultiDateCalendar(this);
  gen_calendar->setGridVisible(true);
  connect(gen_calendar,SIGNAL(markedDatesChanged(int)),
	  this,SLOT(markedDatesChangedData(int)));
  connect(gen_calendar,SIGNAL(selectionChanged()),
	  this,SLOT(selectionChangedData()));
  connect(gen_calendar,SIGNAL(currentPageChanged(int,int)),
	  this,SLOT(pageChangedData(int,int)));

  //
  // Date count label and Clear button
  //
  gen_count_label=new QLabel(tr("0 dates selected"),this);
  gen_count_label->setFont(labelFont());
  gen_count_label->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);

  gen_clear_button=new QPushButton(this);
  gen_clear_button->setFont(subButtonFont());
  gen_clear_button->setText(tr("Clear Selection"));
  connect(gen_clear_button,SIGNAL(clicked()),this,SLOT(clearMarkedData()));

  //
  // Replace Existing Checkbox
  //
  gen_replace_check=new QCheckBox(this);
  gen_replace_check->setText(tr("Replace existing logs without prompting"));
  gen_replace_check->setFont(labelFont());

  //
  // Auto Traffic Merge Checkbox
  //
  gen_merge_traffic_check=new QCheckBox(this);
  gen_merge_traffic_check->setText(tr("Merge traffic if import file is available"));
  gen_merge_traffic_check->setFont(labelFont());

  //
  //  Generate Log Button
  //
  gen_create_button=new QPushButton(this);
  gen_create_button->setFont(buttonFont());
  gen_create_button->setText(tr("Generate Log(s)"));
  gen_create_button->setAutoDefault(false);
  connect(gen_create_button,SIGNAL(clicked()),this,SLOT(createData()));

  //
  //  Merge Music Log Button
  //
  gen_music_button=new QPushButton(this);
  gen_music_button->setFont(buttonFont());
  gen_music_button->setText(tr("Merge Music"));
  connect(gen_music_button,SIGNAL(clicked()),this,SLOT(musicData()));

  //
  //  Merge Traffic Log Button
  //
  gen_traffic_button=new QPushButton(this);
  gen_traffic_button->setFont(buttonFont());
  gen_traffic_button->setText(tr("Merge Traffic"));
  connect(gen_traffic_button,SIGNAL(clicked()),this,SLOT(trafficData()));

  //
  // Status Lights
  //
  gen_import_label=new QLabel(tr("Import Data"),this);
  gen_import_label->setFont(labelFont());
  gen_import_label->setAlignment(Qt::AlignCenter);

  gen_available_label=new QLabel(tr("Available"),this);
  gen_available_label->setFont(subLabelFont());
  gen_available_label->setAlignment(Qt::AlignCenter);

  gen_merged_label=new QLabel(tr("Merged"),this);
  gen_merged_label->setFont(subLabelFont());
  gen_merged_label->setAlignment(Qt::AlignCenter);

  gen_mus_avail_label=new QLabel(this);
  gen_mus_avail_label->
    setPixmap(rda->iconEngine()->listIcon(RDIconEngine::WhiteBall));
  gen_mus_avail_label->setFont(subLabelFont());
  gen_mus_avail_label->setAlignment(Qt::AlignCenter);

  gen_mus_merged_label=new QLabel(this);
  gen_mus_merged_label->
    setPixmap(rda->iconEngine()->listIcon(RDIconEngine::WhiteBall));
  gen_mus_merged_label->setFont(subLabelFont());
  gen_mus_merged_label->setAlignment(Qt::AlignCenter);

  gen_tfc_avail_label=new QLabel(this);
  gen_tfc_avail_label->
    setPixmap(rda->iconEngine()->listIcon(RDIconEngine::WhiteBall));
  gen_tfc_avail_label->setFont(subLabelFont());
  gen_tfc_avail_label->setAlignment(Qt::AlignCenter);

  gen_tfc_merged_label=new QLabel(this);
  gen_tfc_merged_label->
    setPixmap(rda->iconEngine()->listIcon(RDIconEngine::WhiteBall));
  gen_tfc_merged_label->setFont(subLabelFont());
  gen_tfc_merged_label->setAlignment(Qt::AlignCenter);

  //
  //  Close Button
  //
  gen_close_button=new QPushButton(this);
  gen_close_button->setDefault(true);
  gen_close_button->setFont(buttonFont());
  gen_close_button->setText(tr("Close"));
  connect(gen_close_button,SIGNAL(clicked()),this,SLOT(closeData()));

  UpdateControls();

  //
  // File Scan Timer
  //
  QTimer *timer=new QTimer(this);
  connect(timer,SIGNAL(timeout()),this,SLOT(fileScanData()));
  timer->start(GENERATE_LOG_FILESCAN_INTERVAL);

  if(cmdswitch==1 && cmdservicefit) {
    gen_service_box->setCurrentText(*cmdservice);
    gen_calendar->setSelectedDate(*cmddate);
    UpdateControls();
  }
  if(cmdswitch==2 && cmdservicefit) {
    gen_service_box->setCurrentText(*cmdservice);
    gen_calendar->setSelectedDate(*cmddate);
    UpdateControls();
    musicData();
  }
  if(cmdswitch==3 && cmdservicefit) {
    gen_service_box->setCurrentText(*cmdservice);
    gen_calendar->setSelectedDate(*cmddate);
    UpdateControls();
    trafficData();
  }
}


QSize GenerateLog::sizeHint() const
{
  return QSize(420,500);
}


QSizePolicy GenerateLog::sizePolicy() const
{
  return QSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
}


void GenerateLog::serviceActivatedData(int index)
{
  updateCalendarHighlights();
  UpdateControls();
  gen_calendar->setFocus();
}


void GenerateLog::markedDatesChangedData(int count)
{
  if(count==1) {
    gen_count_label->setText(tr("1 date selected"));
  }
  else {
    gen_count_label->setText(QString::number(count)+" "+tr("dates selected"));
  }
  UpdateControls();
}


void GenerateLog::clearMarkedData()
{
  gen_calendar->clearMarked();
  gen_count_label->setText(tr("0 dates selected"));
  UpdateControls();
}


void GenerateLog::selectionChangedData()
{
  UpdateControls();
}


void GenerateLog::pageChangedData(int year,int month)
{
  updateCalendarHighlights();
}


void GenerateLog::createData()
{
  if(gen_calendar->markedCount()==0)
    return;

  QList<QDate> dates;
  foreach(const QDate &d,gen_calendar->markedDates()) {
    dates.append(d);
  }
  std::sort(dates.begin(),dates.end());
  int total=dates.size();

  QString combined_report;
  QString combined_unused;

  gen_progress_dialog=new QProgressDialog(tr("Generating Log..."),
                                           tr("Cancel"),0,24,this);
  gen_progress_dialog->setWindowTitle("RDLogManager");
  gen_progress_dialog->setAutoClose(false);
  gen_progress_dialog->setAutoReset(false);
  gen_progress_dialog->setMinimumDuration(0);
  gen_progress_dialog->show();

  for(int d=0;d<total;d++) {
    QDate date=dates.at(d);

    gen_progress_dialog->setLabelText(
      tr("Generating log for")+" "+date.toString("ddd dd MMM yyyy")+
      " ("+QString::number(d+1)+"/"+QString::number(total)+")");
    gen_progress_dialog->setValue(0);
    QCoreApplication::processEvents();

    RDSvc *svc=
      new RDSvc(gen_service_box->currentText(),rda->station(),rda->config(),this);
    QString logname=RDDateDecode(svc->nameTemplate(),date,
				  rda->station(),rda->config(),svc->name());
    RDLog *log=new RDLog(logname);

    if(log->exists()) {
      if(!gen_replace_check->isChecked()) {
	if(QMessageBox::question(this,"RDLogManager - "+tr("Log Exists"),
				   tr("The log for")+" "+
				   rda->shortDateString(date)+" "+
				   tr("already exists.  Recreating it")+"\n"+
				   tr("will remove any merged Music or Traffic events.")+
				   "\n\n"+tr("Recreate?"),
				   QMessageBox::Yes,QMessageBox::No)!=
	   QMessageBox::Yes) {
	  delete log;
	  delete svc;
	  continue;
	}
      }
      unsigned tracks=0;
      if((tracks=log->completedTracks())>0) {
	if(QMessageBox::warning(this,"RDLogManager - "+tr("Tracks Exist"),
				  tr("This will also delete the")+
				  QString::asprintf(" %u ",tracks)+
				  tr("voice tracks associated with this log.")+
				  "\n"+tr("Continue?"),
				  QMessageBox::Yes,QMessageBox::No)!=
	   QMessageBox::Yes) {
	  delete log;
	  delete svc;
	  continue;
	}
      }
    }

    SendNotification(RDNotification::DeleteAction,log->name());
    log->removeTracks(rda->station(),rda->user(),rda->config());

    srand(QTime::currentTime().msec());
    connect(svc,SIGNAL(generationProgress(int)),
	    gen_progress_dialog,SLOT(setValue(int)));

    QString err_msg;
    QString unused_report;
    if(!svc->generateLog(date,
			  RDDateDecode(svc->nameTemplate(),date,
				       rda->station(),rda->config(),svc->name()),
			  RDDateDecode(svc->nameTemplate(),date.addDays(1),
				       rda->station(),rda->config(),svc->name()),
			  &unused_report,rda->user(),&err_msg)) {
      QMessageBox::warning(this,"RDLogManager - "+tr("Error"),
			    tr("Unable to generate log")+": "+err_msg);
      delete log;
      delete svc;
      if(gen_progress_dialog->wasCanceled())
	break;
      continue;
    }
    log->updateTracks();
    SendNotification(RDNotification::AddAction,log->name());

    if(gen_merge_traffic_check->isChecked()) {
      if(log->linkQuantity(RDLog::SourceTraffic)>0) {
	QString tfc_file=svc->importFilename(RDSvc::Traffic,date);
	if(QFile::exists(tfc_file)) {
	  gen_progress_dialog->setLabelText(
	    tr("Merging traffic for")+" "+date.toString("ddd dd MMM yyyy")+
	    " ("+QString::number(d+1)+"/"+QString::number(total)+")");
	  gen_progress_dialog->setValue(0);
	  QString tfc_report;
	  if(!svc->linkLog(RDSvc::Traffic,date,logname,&tfc_report,
			    rda->user(),&err_msg)) {
	    combined_report+=tr("Traffic merge failed for")+
	      " "+date.toString("ddd dd MMM yyyy")+": "+err_msg+"\n";
	  }
	  else {
	    SendNotification(RDNotification::ModifyAction,log->name());
	  }
	  if(!tfc_report.isEmpty()) {
	    combined_report+=date.toString("ddd dd MMM yyyy")+":\n"+
	      tfc_report+"\n";
	  }
	}
      }
    }

    RDLogModel *model=new RDLogModel(logname,false,this);
    model->load();
    QString day_report;
    int errs=model->validate(&day_report,date);
    if(errs>0||!unused_report.isEmpty()) {
      if(!day_report.isEmpty())
	combined_report+=date.toString("ddd dd MMM yyyy")+":\n"+day_report+"\n";
      if(!unused_report.isEmpty())
	combined_unused+=date.toString("ddd dd MMM yyyy")+":\n"+unused_report+"\n";
    }
    delete model;
    delete log;
    delete svc;

    if(gen_progress_dialog->wasCanceled())
      break;
  }

  delete gen_progress_dialog;
  gen_progress_dialog=nullptr;
  updateCalendarHighlights();

  if(combined_report.isEmpty()&&combined_unused.isEmpty()) {
    QMessageBox::information(this,tr("No Errors"),
			      tr("No broken rules or validation exceptions found."));
  }
  else {
    QString full_report=combined_report;
    if(!combined_unused.isEmpty()) {
      int errs=combined_unused.count("\n");
      full_report+=combined_unused;
      if(errs==1)
	full_report+=QString::asprintf("\n%d broken rule.\n",errs);
      else
	full_report+=QString::asprintf("\n%d broken rules.\n",errs);
    }
    RDTextFile(full_report);
  }

  UpdateControls();
}


void GenerateLog::musicData()
{
  QDate date=gen_calendar->selectedDate();
  unsigned tracks=0;
  QString err_msg;

  RDSvc *svc=
    new RDSvc(gen_service_box->currentText(),rda->station(),rda->config(),this);
  QString logname=RDDateDecode(svc->nameTemplate(),date,
			       rda->station(),rda->config(),svc->name());
  RDLog *log=new RDLog(logname);
  if(((log->linkState(RDLog::SourceMusic)==RDLog::LinkDone)||
      (log->linkState(RDLog::SourceTraffic)==RDLog::LinkDone))) {
    if(log->includeImportMarkers(RDLog::SourceMusic)) {
      if(QMessageBox::question(this,"RDLogManager - "+tr("Music Exists"),
			       tr("The log for")+" "+
			       rda->shortDateString(date)+" "+
			       tr("already contains merged music and/or traffic data.")+"\n"+
			       tr("Remerging it will remove this data.  Remerge?"),
			       QMessageBox::Yes,QMessageBox::No)!=
	 QMessageBox::Yes) {
	delete log;
	delete svc;
	return;
      }
      if((tracks=log->completedTracks())>0) {
	if(QMessageBox::warning(this,"RDLogManager - "+tr("Tracks Exist"),
				tr("This will also delete the")+
				QString::asprintf(" %u ",tracks)+
				tr("voice tracks associated with this log.")+
				"\n"+tr("Continue?"),
				QMessageBox::Yes,QMessageBox::No)!=
	   QMessageBox::Yes) {
	  delete log;
	  delete svc;
	  return;
	}
      }
    }
    else {
      QMessageBox::warning(this,"RDLogManager - "+tr("Error"),
			   tr("The log for")+" "+
			   rda->shortDateString(date)+" "+
			   tr("cannot be relinked."));
      return;
    }
    log->removeTracks(rda->station(),rda->user(),rda->config());
    if(!svc->clearLogLinks(RDSvc::Traffic,logname,rda->user(),&err_msg)) {
      QMessageBox::warning(this,"RDLogManager - "+tr("Error"),
			   tr("Unable to clear traffic links")+": "+err_msg);
      delete log;
      delete svc;
      return;
    }
    if(!svc->clearLogLinks(RDSvc::Music,logname,rda->user(),&err_msg)) {
      QMessageBox::warning(this,"RDLogManager - "+tr("Error"),
			   tr("Unable to clear music links")+": "+err_msg);
      delete log;
      delete svc;
      return;
    }
  }
  gen_progress_dialog=new QProgressDialog(tr("Merging Music..."),
                                           QString(),0,24,this);
  gen_progress_dialog->setWindowTitle("RDLogManager");
  gen_progress_dialog->setAutoClose(false);
  gen_progress_dialog->setAutoReset(false);
  gen_progress_dialog->setMinimumDuration(0);
  gen_progress_dialog->show();
  connect(svc,SIGNAL(generationProgress(int)),
	  gen_progress_dialog,SLOT(setValue(int)));
  QString report;
  if(!svc->linkLog(RDSvc::Music,date,logname,&report,
		   rda->user(),&err_msg)) {
    delete gen_progress_dialog;
    gen_progress_dialog=nullptr;
    RDTextFile(tr("RDLogManager Error Report")+"\n\n"+
	       tr("Music schedule import failed!")+"\n\n"+err_msg);
    delete log;
    delete svc;
    UpdateControls();
    return;
  }
  delete gen_progress_dialog;
  gen_progress_dialog=nullptr;
  SendNotification(RDNotification::ModifyAction,log->name());
  delete log;
  delete svc;
  if(!report.isEmpty()) {
    RDTextFile(report);
  }
  updateCalendarHighlights();
  UpdateControls();
}


void GenerateLog::trafficData()
{
  QDate date=gen_calendar->selectedDate();
  QString err_msg;

  RDSvc *svc=
    new RDSvc(gen_service_box->currentText(),rda->station(),rda->config(),this);
  QString logname=RDDateDecode(svc->nameTemplate(),date,
			       rda->station(),rda->config(),svc->name());
  RDLog *log=new RDLog(logname);
  if((log->linkState(RDLog::SourceTraffic)==RDLog::LinkDone)) {
    if(log->includeImportMarkers(RDLog::SourceTraffic)) {
      if(QMessageBox::question(this,"RDLogManager - "+tr("Traffic Exists"),
			       tr("The log for")+" "+
			       rda->shortDateString(date)+" "+
			       tr("already contains merged traffic data.")+"\n"+
			       tr("Remerging it will remove this data.  Remerge?"),
			       QMessageBox::Yes,QMessageBox::No)!=
	 QMessageBox::Yes) {
	delete log;
	delete svc;
	return;
      }
    }
    else {
      QMessageBox::warning(this,"RDLogManager - "+tr("Error"),
			   tr("The log for")+" "+
			   rda->shortDateString(date)+" "+
			   tr("cannot be relinked."));
      return;
    }
    if(!svc->clearLogLinks(RDSvc::Traffic,logname,rda->user(),&err_msg)) {
      QMessageBox::warning(this,"RDLogManager - "+tr("Error"),
			   tr("Unable to clear traffic links")+": "+err_msg);
      delete log;
      delete svc;
      return;
    }
  }
  gen_progress_dialog=new QProgressDialog(tr("Merging Traffic..."),
                                           QString(),0,24,this);
  gen_progress_dialog->setWindowTitle("RDLogManager");
  gen_progress_dialog->setAutoClose(false);
  gen_progress_dialog->setAutoReset(false);
  gen_progress_dialog->setMinimumDuration(0);
  gen_progress_dialog->show();
  connect(svc,SIGNAL(generationProgress(int)),
	  gen_progress_dialog,SLOT(setValue(int)));
  QString report;
  if(!svc->linkLog(RDSvc::Traffic,date,logname,&report,rda->user(),
		   &err_msg)) {
    delete gen_progress_dialog;
    gen_progress_dialog=nullptr;
    RDTextFile(tr("RDLogManager Error Report")+"\n\n"+
	       tr("Traffic schedule import failed!")+"\n\n"+err_msg);
    delete log;
    delete svc;
    UpdateControls();
    return;
  }
  delete gen_progress_dialog;
  gen_progress_dialog=nullptr;
  SendNotification(RDNotification::ModifyAction,log->name());
  delete log;
  delete svc;
  if(!report.isEmpty()) {
    RDTextFile(report);
  }
  updateCalendarHighlights();
  UpdateControls();
}


void GenerateLog::fileScanData()
{
  if(gen_service_box->currentIndex()==0)
    return;

  QDate date=gen_calendar->selectedDate();
  RDSvc *svc=new RDSvc(gen_service_box->currentText(),rda->station(),
			rda->config(),this);
  QString logname=RDDateDecode(svc->nameTemplate(),date,
			       rda->station(),rda->config(),svc->name());
  RDLog *log=new RDLog(logname);
  if(gen_music_enabled) {
    if(QFile::exists(svc->importFilename(RDSvc::Music,date))) {
      gen_music_button->
	setEnabled(log->includeImportMarkers(RDLog::SourceMusic)||
		   (log->linkState(RDLog::SourceMusic)==RDLog::LinkMissing));
      gen_mus_avail_label->
	setPixmap(rda->iconEngine()->listIcon(RDIconEngine::GreenBall));
    }
    else {
      gen_music_button->setDisabled(true);
      gen_mus_avail_label->
	setPixmap(rda->iconEngine()->listIcon(RDIconEngine::RedBall));
    }
  }
  else {
    gen_mus_avail_label->
      setPixmap(rda->iconEngine()->listIcon(RDIconEngine::WhiteBall));
  }
  if(gen_traffic_enabled) {
    if(QFile::exists(svc->importFilename(RDSvc::Traffic,date))) {
      gen_traffic_button->
	setEnabled(((!gen_music_enabled)||
		    (log->linkState(RDLog::SourceMusic)==RDLog::LinkDone))&&
		   (log->includeImportMarkers(RDLog::SourceTraffic)||
		    (log->linkState(RDLog::SourceTraffic)==
		     RDLog::LinkMissing)));
      gen_tfc_avail_label->
	setPixmap(rda->iconEngine()->listIcon(RDIconEngine::GreenBall));
    }
    else {
      gen_traffic_button->setDisabled(true);
      gen_tfc_avail_label->
	setPixmap(rda->iconEngine()->listIcon(RDIconEngine::RedBall));
    }
  }
  else {
    gen_tfc_avail_label->
      setPixmap(rda->iconEngine()->listIcon(RDIconEngine::WhiteBall));
  }
  delete log;
  delete svc;
}


void GenerateLog::closeData()
{
  done(0);
}


void GenerateLog::resizeEvent(QResizeEvent *e)
{
  int w=sizeHint().width();
  int h=sizeHint().height();

  gen_service_label->setGeometry(10,10,55,20);
  gen_service_box->setGeometry(70,10,w-80,20);

  gen_calendar->setGeometry(10,38,w-20,185);

  gen_count_label->setGeometry(10,233,220,20);
  gen_clear_button->setGeometry(238,229,w-248,28);

  gen_replace_check->setGeometry(10,265,w-20,20);
  gen_merge_traffic_check->setGeometry(10,290,w-20,20);

  gen_create_button->setGeometry(10,318,w-20,30);

  gen_import_label->setGeometry(130,360,210,14);
  gen_available_label->setGeometry(200,374,60,14);
  gen_merged_label->setGeometry(268,374,60,14);

  gen_music_button->setGeometry(10,358,110,30);
  gen_mus_avail_label->setGeometry(200,388,60,14);
  gen_mus_merged_label->setGeometry(268,388,60,14);

  gen_traffic_button->setGeometry(10,398,110,30);
  gen_tfc_avail_label->setGeometry(200,412,60,14);
  gen_tfc_merged_label->setGeometry(268,412,60,14);

  gen_close_button->setGeometry(10,h-60,w-20,50);
}


void GenerateLog::UpdateControls()
{
  bool svc_ok=gen_service_box->currentIndex()>0;
  bool has_marked=gen_calendar->markedCount()>0;

  gen_calendar->setEnabled(svc_ok);
  gen_count_label->setEnabled(svc_ok);
  gen_clear_button->setEnabled(svc_ok&&has_marked);
  gen_replace_check->setEnabled(svc_ok);
  gen_merge_traffic_check->setEnabled(svc_ok);
  gen_create_button->setEnabled(svc_ok&&has_marked);
  gen_import_label->setEnabled(svc_ok);
  gen_available_label->setEnabled(svc_ok);
  gen_merged_label->setEnabled(svc_ok);
  gen_mus_avail_label->setEnabled(svc_ok);
  gen_mus_merged_label->setEnabled(svc_ok);
  gen_tfc_avail_label->setEnabled(svc_ok);
  gen_tfc_merged_label->setEnabled(svc_ok);

  if(!svc_ok) {
    gen_music_button->setDisabled(true);
    gen_traffic_button->setDisabled(true);
    gen_mus_merged_label->
      setPixmap(rda->iconEngine()->listIcon(RDIconEngine::WhiteBall));
    gen_tfc_merged_label->
      setPixmap(rda->iconEngine()->listIcon(RDIconEngine::WhiteBall));
    gen_mus_avail_label->
      setPixmap(rda->iconEngine()->listIcon(RDIconEngine::WhiteBall));
    gen_tfc_avail_label->
      setPixmap(rda->iconEngine()->listIcon(RDIconEngine::WhiteBall));
    gen_music_enabled=false;
    gen_traffic_enabled=false;
    return;
  }

  QDate date=gen_calendar->selectedDate();
  RDSvc *svc=new RDSvc(gen_service_box->currentText(),rda->station(),
			rda->config(),this);
  QString logname=RDDateDecode(svc->nameTemplate(),date,
			       rda->station(),rda->config(),svc->name());
  RDLog *log=new RDLog(logname);
  if(log->exists()) {
    if(log->linkQuantity(RDLog::SourceMusic)>0) {
      gen_music_enabled=true;
      if(log->linkState(RDLog::SourceMusic)==RDLog::LinkDone) {
	gen_mus_merged_label->
	  setPixmap(rda->iconEngine()->listIcon(RDIconEngine::GreenBall));
      }
      else {
	gen_mus_merged_label->
	  setPixmap(rda->iconEngine()->listIcon(RDIconEngine::RedBall));
      }
    }
    else {
      gen_music_enabled=false;
      gen_music_button->setDisabled(true);
      gen_mus_merged_label->
	setPixmap(rda->iconEngine()->listIcon(RDIconEngine::WhiteBall));
    }
    if(log->linkQuantity(RDLog::SourceTraffic)>0) {
      gen_traffic_enabled=true;
      if(log->linkState(RDLog::SourceTraffic)==RDLog::LinkDone) {
	gen_tfc_merged_label->
	  setPixmap(rda->iconEngine()->listIcon(RDIconEngine::GreenBall));
      }
      else {
	gen_tfc_merged_label->
	  setPixmap(rda->iconEngine()->listIcon(RDIconEngine::RedBall));
      }
    }
    else {
      gen_traffic_enabled=false;
      gen_traffic_button->setDisabled(true);
      gen_tfc_merged_label->
	setPixmap(rda->iconEngine()->listIcon(RDIconEngine::WhiteBall));
    }
  }
  else {
    gen_music_button->setDisabled(true);
    gen_mus_merged_label->
      setPixmap(rda->iconEngine()->listIcon(RDIconEngine::WhiteBall));
    gen_traffic_button->setDisabled(true);
    gen_tfc_merged_label->
      setPixmap(rda->iconEngine()->listIcon(RDIconEngine::WhiteBall));
    gen_music_enabled=false;
    gen_traffic_enabled=false;
  }
  delete log;
  delete svc;
  fileScanData();
}


void GenerateLog::updateCalendarHighlights()
{
  if(gen_service_box->currentIndex()==0) {
    gen_calendar->setLogStatuses(QMap<QDate,int>());
    return;
  }

  RDSvc *svc=new RDSvc(gen_service_box->currentText(),rda->station(),
			rda->config(),this);

  // One query for all logs belonging to this service
  QString sql=QString("select `NAME`,`TRAFFIC_LINKED` from `LOGS` where ")+
    "`SERVICE`='"+RDEscapeString(svc->name())+"'";
  RDSqlQuery *q=new RDSqlQuery(sql);
  QMap<QString,int> byName;  // logname -> 1=generated, 2=traffic merged
  while(q->next()) {
    int status=(q->value(1).toString()=="Y") ? 2 : 1;
    byName[q->value(0).toString()]=status;
  }
  delete q;

  // Map log names back to dates for the visible month range
  // Go back 6 days to cover any first-day-of-week and forward 42 days
  QDate first(gen_calendar->yearShown(),gen_calendar->monthShown(),1);
  QDate rangeStart=first.addDays(-6);
  QDate rangeEnd=first.addDays(41);

  QMap<QDate,int> statusMap;
  for(QDate d=rangeStart;d<=rangeEnd;d=d.addDays(1)) {
    QString logname=RDDateDecode(svc->nameTemplate(),d,
				  rda->station(),rda->config(),svc->name());
    if(byName.contains(logname)) {
      statusMap[d]=byName[logname];
    }
  }

  gen_calendar->setLogStatuses(statusMap);
  delete svc;
}


void GenerateLog::SendNotification(RDNotification::Action action,
				   const QString &logname)
{
  RDNotification *notify=new RDNotification(RDNotification::LogType,
					    action,QVariant(logname));
  rda->ripc()->sendNotification(*notify);
  delete notify;
}
