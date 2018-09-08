#include "about.h"

#include <QCoreApplication>
#include <QString>

#include "revision_utils.hpp"

#include "ui_about.h"

CAboutDlg::CAboutDlg(QWidget *parent) :
  QDialog(parent),
  ui(new Ui::CAboutDlg)
{
  ui->setupUi(this);

  ui->labelTxt->setText ("<h2>" + QString {"FT8Call v"
                             + QCoreApplication::applicationVersion ()
                             + " " + revision ()}.simplified () + "</h2><br />"

                         "FT8Call is a derivative of the WSJT-X application, "
                         "restructured and redesigned for message passing. <br/>"
                         "It is not supported by nor endorsed by the WSJT-X "
                         "development group. <br/>FT8Call is "
                         "licensed under and in accordance with the terms "
                         "of the <a href=\"https://www.gnu.org/licenses/gpl-3.0.txt\">GPLv3 license</a>.<br/>"
                         "The source code modifications are public and can be found in <a href=\"https://bitbucket.org/widefido/wsjtx/\">this repository</a>.<br/><br/>"

                         "FT8Call is heavily inspired by WSJT-X, Fldigi, "
                         "and FSQCall <br/>and would not exist without the hard work and "
                         "dedication of the many <br/>developers in the amateur radio "
                         "community.<br /><br />"
                         "FT8Call stands on the shoulder of giants...the takeoff angle "
                         "is better up there.<br /><br />"
                         "A special thanks goes out to the FT8Call development team:<br/><br/><strong>"
                         "KC9QNE, "
                         "KI6SSI, "
                         "LB9YH, "
                         "M0IAX, "
                         "N0JDS, "
                         "OH8STN, "
                         "VA3OSO, "
                         "VK1MIC, "
                         "W0FW,</strong><br/><br/>and the many other amateur radio operators who have given "
                         "FT8Call a chance.");
}

CAboutDlg::~CAboutDlg()
{
}
