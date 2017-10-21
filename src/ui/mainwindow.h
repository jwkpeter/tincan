#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H


#include <QMainWindow>
#include <QChartView>

#include "network/canudpreceiver.h"
#include "tincan/canbus.h"
#include "tincan/canbusdef.h"
#include "tincan/cantracer.h"
#include "file/dbcfile.h"
#include "models/canbusmodel.h"
#include "models/canbusdefmodel.h"


namespace Ui { class MainWindow; }


class Main_window : public QMainWindow
{
  Q_OBJECT

public:
  explicit Main_window(QWidget* parent = nullptr);
  ~Main_window();

private:
  Ui::MainWindow* ui;
  QtCharts::QChartView chart_view_;
  can::Udp_receiver can_udp_receiver_;
  dbc::File dbc_file_;
  tin::Can_bus can_bus_;
  tin::Can_bus_def can_bus_def_;
  tin::Can_bus_model can_bus_model_;
  tin::Can_bus_def_model can_bus_def_model_;
  tin::Can_tracer can_tracer_;
};


#endif  // MAIN_WINDOW_H
