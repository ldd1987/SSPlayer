#include "SSPlayer.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	SSPlayer w;
	w.show();
	return a.exec();
}
