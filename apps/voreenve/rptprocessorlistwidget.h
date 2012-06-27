/**********************************************************************
 *                                                                    *
 * Voreen - The Volume Rendering Engine                               *
 *                                                                    *
 * Copyright (C) 2005-2008 Visualization and Computer Graphics Group, *
 * Department of Computer Science, University of Muenster, Germany.   *
 * <http://viscg.uni-muenster.de>                                     *
 *                                                                    *
 * This file is part of the Voreen software package. Voreen is free   *
 * software: you can redistribute it and/or modify it under the terms *
 * of the GNU General Public License version 2 as published by the    *
 * Free Software Foundation.                                          *
 *                                                                    *
 * Voreen is distributed in the hope that it will be useful,          *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of     *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the       *
 * GNU General Public License for more details.                       *
 *                                                                    *
 * You should have received a copy of the GNU General Public License  *
 * in the file "LICENSE.txt" along with this program.                 *
 * If not, see <http://www.gnu.org/licenses/>.                        *
 *                                                                    *
 * The authors reserve all rights not expressly granted herein. For   *
 * non-commercial academic use see the license exception specified in *
 * the file "LICENSE-academic.txt". To get information about          *
 * commercial licensing please contact the authors.                   *
 *                                                                    *
 **********************************************************************/

#ifndef RPT_PROCESSORLISTWIDGET_H
#define RPT_PROCESSORLISTWIDGET_H

#include <QtGui>
#include "voreen/core/vis/identifier.h"

namespace voreen {

class RptProcessorListWidget : public QTreeWidget {

	Q_OBJECT

public:
    RptProcessorListWidget(QWidget* parent=0);

protected:
    void mousePressEvent(QMouseEvent *event);

private:
    void buildItems();
    
    std::vector<Identifier> processorVector_;
    QList<QTreeWidgetItem *> items_;


};

//---------------------------------------------------------------------------

class RptProcessorListItem : public QTreeWidgetItem {

public:
    RptProcessorListItem(Identifier id);

    Identifier getId() {
        return id_;
    }

private:
    Identifier id_;

};

} //namespace voreen

#endif // RPT_PROCESSORLISTWIDGET_H