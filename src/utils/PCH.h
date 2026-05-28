//
// Created by 66 on 2025/8/21.
//

#ifndef PCH_H
#define PCH_H

#pragma once

//qt
#include <QApplication>
#include <QtCore>
#include <QtGui>
#include <QtWidgets>

//c++ standard
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <memory>
#include <string>

#endif //PCH_H

#ifdef _DEBUG
#define VLD_FORCE_COPY 1
#include <vld.h>
#endif
