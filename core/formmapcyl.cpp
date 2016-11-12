#include "formmapcyl.h"
#include "ui_formmapcyl.h"

extern Options opts;
extern SegmentImage *imageptrs;


FormMapCyl::FormMapCyl(QWidget *parent, MapFieldCyl *p_mapcyl, Globe *p_globe, FormToolbox *p_formtoolbox, SatelliteList *satlist, AVHRRSatellite *seglist ) :
    QWidget(parent),
    ui(new Ui::FormMapCyl)
{
    ui->setupUi(this);

    qDebug() << "constructor formmapcyl";

    sats = satlist;
    segs = seglist;
    mapcyl = p_mapcyl;
    globe = p_globe;
    formtoolbox = p_formtoolbox;

    ui->stackedWidget->addWidget(mapcyl);
    ui->stackedWidget->addWidget(globe);

    ui->stackedWidget->setCurrentIndex(0);

    ui->btnMetop->setCheckable(true);
    ui->btnNoaa->setCheckable(true);
    ui->btnGAC->setCheckable(true);
    ui->btnHRP->setCheckable(true);
    ui->btnVIIRSM->setCheckable(true);
    ui->btnVIIRSDNB->setCheckable(true);
    ui->btnOLCIefr->setCheckable(true);
    ui->btnOLCIerr->setCheckable(true);
    ui->btnRealTime->setCheckable(true);
    ui->btnPhong->setCheckable(true);


    if (opts.buttonMetop)
    {
        opts.buttonNoaa = false;
        opts.buttonGAC = false;
        opts.buttonHRP = false;
        opts.buttonVIIRSM = false;
        opts.buttonVIIRSDNB = false;
        opts.buttonOLCIefr = false;
        opts.buttonOLCIerr = false;
        opts.buttonRealTime = false;
    }
    else
        if (opts.buttonNoaa)
        {
            opts.buttonMetop = false;
            opts.buttonGAC = false;
            opts.buttonHRP = false;
            opts.buttonVIIRSM = false;
            opts.buttonVIIRSDNB = false;
            opts.buttonOLCIefr = false;
            opts.buttonOLCIerr = false;
            opts.buttonRealTime = false;
        }
        else
            if (opts.buttonGAC)
            {
                opts.buttonNoaa = false;
                opts.buttonMetop = false;
                opts.buttonHRP = false;
                opts.buttonVIIRSM = false;
                opts.buttonVIIRSDNB = false;
                opts.buttonOLCIefr = false;
                opts.buttonOLCIerr = false;
                opts.buttonRealTime = false;
            }
            else
                if (opts.buttonHRP)
                {
                    opts.buttonNoaa = false;
                    opts.buttonGAC = false;
                    opts.buttonMetop = false;
                    opts.buttonVIIRSM = false;
                    opts.buttonVIIRSDNB = false;
                    opts.buttonOLCIefr = false;
                    opts.buttonOLCIerr = false;
                    opts.buttonRealTime = false;
                }
                else
                    if (opts.buttonRealTime)
                    {
                        opts.buttonNoaa = false;
                        opts.buttonGAC = false;
                        opts.buttonHRP = false;
                        opts.buttonMetop = false;
                        opts.buttonVIIRSM = false;
                        opts.buttonVIIRSDNB = false;
                        opts.buttonOLCIefr = false;
                        opts.buttonOLCIerr = false;
                    }
                    else
                        if (opts.buttonVIIRSM)
                        {
                            opts.buttonNoaa = false;
                            opts.buttonGAC = false;
                            opts.buttonHRP = false;
                            opts.buttonMetop = false;
                            opts.buttonRealTime = false;
                            opts.buttonVIIRSDNB = false;
                            opts.buttonOLCIefr = false;
                            opts.buttonOLCIerr = false;
                            formtoolbox->setTabWidgetIndex(TAB_VIIRS);
                            formtoolbox->setTabWidgetVIIRSIndex(0);
                        }
                        else
                            if (opts.buttonVIIRSDNB)
                            {
                                opts.buttonNoaa = false;
                                opts.buttonGAC = false;
                                opts.buttonHRP = false;
                                opts.buttonMetop = false;
                                opts.buttonRealTime = false;
                                opts.buttonVIIRSM = false;
                                opts.buttonOLCIefr = false;
                                opts.buttonOLCIerr = false;
                                formtoolbox->setTabWidgetIndex(TAB_VIIRS);
                                formtoolbox->setTabWidgetVIIRSIndex(1);
                            }
                            else
                                if (opts.buttonOLCIefr)
                                {
                                    opts.buttonNoaa = false;
                                    opts.buttonGAC = false;
                                    opts.buttonHRP = false;
                                    opts.buttonMetop = false;
                                    opts.buttonRealTime = false;
                                    opts.buttonVIIRSM = false;
                                    opts.buttonVIIRSDNB = false;
                                    opts.buttonOLCIerr = false;
                                    formtoolbox->setTabWidgetIndex(TAB_OLCI);
                                }
                                else
                                    if (opts.buttonOLCIerr)
                                    {
                                        opts.buttonNoaa = false;
                                        opts.buttonGAC = false;
                                        opts.buttonHRP = false;
                                        opts.buttonMetop = false;
                                        opts.buttonRealTime = false;
                                        opts.buttonVIIRSM = false;
                                        opts.buttonVIIRSDNB = false;
                                        opts.buttonOLCIefr = false;
                                        formtoolbox->setTabWidgetIndex(TAB_OLCI);
                                    }

    ui->btnMetop->setChecked(opts.buttonMetop);
    ui->btnNoaa->setChecked(opts.buttonNoaa);
    ui->btnGAC->setChecked(opts.buttonGAC);
    ui->btnHRP->setChecked(opts.buttonHRP);
    ui->btnVIIRSM->setChecked(opts.buttonVIIRSM);
    ui->btnVIIRSDNB->setChecked(opts.buttonVIIRSDNB);
    ui->btnOLCIefr->setChecked(opts.buttonOLCIefr);
    ui->btnOLCIerr->setChecked(opts.buttonOLCIerr);
    ui->btnRealTime->setChecked(opts.buttonRealTime);
    ui->btnPhong->setChecked(true);


//    connect( ui->btnMetop, SIGNAL( clicked() ), mapcyl, SLOT( showMetopSegments() ) );
//    connect( ui->btnNoaa, SIGNAL( clicked() ), mapcyl, SLOT( showNoaaSegments() ));
//    connect( ui->btnGAC, SIGNAL( clicked() ), mapcyl, SLOT( showGACSegments() ));
//    connect( ui->btnHRP, SIGNAL( clicked() ), mapcyl, SLOT( showHRPSegments() ));
//    connect( ui->btnVIIRSM, SIGNAL( clicked() ), mapcyl, SLOT( showVIIRSSegments() ));
//    connect( ui->btnVIIRSDNB, SIGNAL( clicked() ), mapcyl, SLOT( showVIIRSSegments() ));

    connect( ui->btnMetop, SIGNAL( clicked() ), formtoolbox, SLOT( setChannelComboBoxes() ) );
    connect( ui->btnNoaa, SIGNAL( clicked() ), formtoolbox, SLOT( setChannelComboBoxes() ));
    connect( ui->btnGAC, SIGNAL( clicked() ), formtoolbox, SLOT( setChannelComboBoxes() ));
    connect( ui->btnHRP, SIGNAL( clicked() ), formtoolbox, SLOT( setChannelComboBoxes() ));
    connect( ui->btnVIIRSM, SIGNAL( clicked() ), formtoolbox, SLOT( setChannelComboBoxes() ));
    connect( ui->btnVIIRSDNB, SIGNAL( clicked() ), formtoolbox, SLOT( setChannelComboBoxes() ));

    //connect(ui->verticalScrollBar, SIGNAL(valueChanged(int)), this, SLOT(showSegmentList(int)));
    connect(mapcyl, SIGNAL(wheelChange(int)), this, SLOT(changeScrollBar(int)));
    connect(mapcyl, SIGNAL(mapClicked()), this, SLOT(showSegmentcount()));
    connect(globe, SIGNAL(mapClicked()), this, SLOT(showSegmentcount()));

    this->showSegmentcount();

}

// Key handler
void FormMapCyl::keyPressEvent(QKeyEvent *event)
{

    qDebug() << "FormMapCyl::keyPressEvent(QKeyEvent *event)";

    switch (event->key())
    {

    default:
        globe->keyPressEvent(event);
    }
}
void FormMapCyl::setCylOrGlobe(int ind)
{
    ui->stackedWidget->setCurrentIndex(ind);
    qDebug() << QString("setCylOrGlobe = %1").arg(ind);
    if (ind == 0)
        ui->btnSaveTexture->setVisible(false);
    else
        ui->btnSaveTexture->setVisible(true);
    qDebug() << QString("na setCylOrGlobe = %1").arg(ind);

}

void FormMapCyl::showSegmentcount()
{
    qDebug() << "FormMapCyl::showSegmentcount";

    int cntselmetop = segs->seglmetop->NbrOfSegmentsSelected();
    int cntselnoaa = segs->seglnoaa->NbrOfSegmentsSelected();
    int cntselhrp = segs->seglhrp->NbrOfSegmentsSelected();
    int cntselgac = segs->seglgac->NbrOfSegmentsSelected();
    int cntselviirsm = segs->seglviirsm->NbrOfSegmentsSelected();
    int cntselviirsdnb = segs->seglviirsdnb->NbrOfSegmentsSelected();
    int cntselolciefr = segs->seglolciefr->NbrOfSegmentsSelected();
    int cntselolcierr = segs->seglolcierr->NbrOfSegmentsSelected();

    int cntmetop = segs->seglmetop->NbrOfSegments();
    int cntnoaa = segs->seglnoaa->NbrOfSegments();
    int cnthrp = segs->seglhrp->NbrOfSegments();
    int cntgac = segs->seglgac->NbrOfSegments();
    int cntviirsm = segs->seglviirsm->NbrOfSegments();
    int cntviirsdnb = segs->seglviirsdnb->NbrOfSegments();
    int cntolciefr = segs->seglolciefr->NbrOfSegments();
    int cntolcierr = segs->seglolcierr->NbrOfSegments();

    long totseg = cntmetop + cntnoaa + cnthrp + cntgac + cntviirsm + cntviirsdnb + cntolciefr + cntolcierr;
    long totsegsel = cntselmetop + cntselnoaa + cntselhrp + cntselgac + cntselviirsm + cntselviirsdnb + cntselolciefr + cntselolcierr;

    if ( totsegsel  > 0)
    {
        ui->btnRemoveSelected->setText( QString(" Remove %1 selected segments ").arg(totsegsel));
    }
    else
    {
        ui->btnRemoveSelected->setText(" No selected segments ");
    }

    ui->btnMetop->setText((QString("Metop tracks # %1/%2").arg(cntselmetop).arg(cntmetop)));
    ui->btnNoaa->setText((QString("Noaa tracks # %1/%2").arg(cntselnoaa).arg(cntnoaa)));
    ui->btnGAC->setText((QString("GAC tracks # %1/%2").arg(cntselgac).arg(cntgac)));
    ui->btnHRP->setText((QString("HRP tracks # %1/%2").arg(cntselhrp).arg(cnthrp)));

    ui->btnVIIRSM->setText((QString("VIIRS M tracks # %1/%2").arg(cntselviirsm).arg(cntviirsm)));
    ui->btnVIIRSDNB->setText((QString("VIIRS DNB tracks # %1/%2").arg(cntselviirsdnb).arg(cntviirsdnb)));

    ui->btnOLCIefr->setText((QString("S3A OLCI EFR tracks # %1/%2").arg(cntselolciefr).arg(cntolciefr)));
    ui->btnOLCIerr->setText((QString("S3A OLCI ERR tracks # %1/%2").arg(cntselolcierr).arg(cntolcierr)));


}

void FormMapCyl::changeScrollBar(int value)
{
    qDebug() << QString("---------changeScrollBar value = %1").arg(value);
    ui->verticalScrollBar->setValue(ui->verticalScrollBar->value() + value);
}

void FormMapCyl::updatesatmap(int index)
{
    qDebug() << QString("stack index %1 selected scrollbar value %2").arg(index).arg(ui->verticalScrollBar->value());

    if (index == 0) //ephem
    {

    }
    if (index == 1 ) //cylequidist || Globe
    {
        if (ui->verticalScrollBar->value() == -1)
            ui->verticalScrollBar->setValue(0);

        this->setScrollBarMaximum();

        if (opts.buttonMetop )
        {
            segs->seglmetop->ShowSegment(ui->verticalScrollBar->value());
        }
        if (opts.buttonNoaa )
        {
            segs->seglnoaa->ShowSegment(ui->verticalScrollBar->value());
        }
        if (opts.buttonHRP)
        {
            segs->seglhrp->ShowSegment(ui->verticalScrollBar->value());
        }
        if (opts.buttonGAC)
        {
            segs->seglgac->ShowSegment(ui->verticalScrollBar->value());
        }
        if (opts.buttonVIIRSM)
        {
            segs->seglviirsm->ShowSegment(ui->verticalScrollBar->value());
        }
        if (opts.buttonVIIRSDNB)
        {
            segs->seglviirsdnb->ShowSegment(ui->verticalScrollBar->value());
        }
        if (opts.buttonOLCIefr)
        {
            segs->seglolciefr->ShowSegment(ui->verticalScrollBar->value());
        }
        if (opts.buttonOLCIerr)
        {
            segs->seglolcierr->ShowSegment(ui->verticalScrollBar->value());
        }

        mapcyl->update();
    }
    if (index == 2)  //imagetab
    {
        QString tit;

        if (opts.buttonMetop )
        {
            tit = "Metop ";
        } else if (opts.buttonNoaa )
        {
             tit = "Noaa ";
        } else if (opts.buttonGAC)
        {
             tit = "GAC ";
        } else if (opts.buttonHRP)
        {
             tit = "HRP ";
        } else if (opts.buttonVIIRSM)
        {
            tit = "VIIRSM ";
        }  else if (opts.buttonVIIRSDNB)
        {
            tit = "VIIRSDNB ";
        } else if (opts.buttonOLCIefr)
        {
            tit = "OLCI EFR ";
        }  else if (opts.buttonOLCIerr)
        {
            tit = "OLCI ERR ";
        }
/*
        switch(imagetab->GetChannelShown())
        {
        case 1:
            tit += "Channel 1";
            break;
        case 2:
            tit += "Channel 2";
            break;
        case 3:
            tit += "Channel 3";
            break;
        case 4:
            tit += "Channel 4";
            break;
        case 5:
            tit += "Channel 5";
            break;
        case 6:
            tit += "Color image";
            break;
        case 7:
            tit = "Expanded Color image";
            break;
        }

        qDebug() << QString("channelshown = %1").arg(imagetab->GetChannelShown());

        tabWidget->setTabText(4, tit);
*/
     }
}

void FormMapCyl::toggleButton(eSegmentType segtype)
{

    opts.buttonMetop = segtype == eSegmentType::SEG_METOP ? true : false;
    opts.buttonNoaa = segtype == eSegmentType::SEG_NOAA ? true : false;
    opts.buttonGAC = segtype == eSegmentType::SEG_GAC ? true : false;
    opts.buttonHRP = segtype == eSegmentType::SEG_HRP ? true : false;
    opts.buttonVIIRSM = segtype == eSegmentType::SEG_VIIRSM ? true : false;
    opts.buttonVIIRSDNB = segtype == eSegmentType::SEG_VIIRSDNB ? true : false;
    opts.buttonOLCIefr = segtype == eSegmentType::SEG_OLCIEFR ? true : false;
    opts.buttonOLCIerr = segtype == eSegmentType::SEG_OLCIERR ? true : false;
    opts.buttonRealTime = segtype == eSegmentType::SEG_NONE ? true : false;

    ui->btnMetop->setChecked(opts.buttonMetop);
    ui->btnNoaa->setChecked(opts.buttonNoaa);
    ui->btnGAC->setChecked(opts.buttonGAC);
    ui->btnHRP->setChecked(opts.buttonHRP);
    ui->btnVIIRSM->setChecked(opts.buttonVIIRSM);
    ui->btnVIIRSDNB->setChecked(opts.buttonVIIRSDNB);
    ui->btnOLCIefr->setChecked(opts.buttonOLCIefr);
    ui->btnOLCIerr->setChecked(opts.buttonOLCIerr);
    ui->btnRealTime->setChecked(opts.buttonRealTime);
    this->showSegmentList(0);

    //imagetab->SetGammaSpinboxes();
    this->RemoveAllSelected();
    this->setScrollBarMaximum();


    return;
}


void FormMapCyl::setScrollBarMaximum()
{


    if (opts.buttonMetop)
    {
        ui->verticalScrollBar->setMaximum(segs->seglmetop->NbrOfSegments());
        qDebug() << QString("setscrollbarmaximum metop = %1").arg(segs->seglmetop->NbrOfSegments());
    }
    else if (opts.buttonNoaa)
    {
        ui->verticalScrollBar->setMaximum(segs->seglnoaa->NbrOfSegments());
        qDebug() << QString("setscrollbarmaximum noaa = %1").arg(segs->seglnoaa->NbrOfSegments());
    }
    else if (opts.buttonHRP)
    {
        ui->verticalScrollBar->setMaximum(segs->seglhrp->NbrOfSegments());
        qDebug() << QString("setscrollbarmaximum HRP = %1").arg(segs->seglhrp->NbrOfSegments());
    }
    else if (opts.buttonGAC)
    {
        ui->verticalScrollBar->setMaximum(segs->seglgac->NbrOfSegments());
        qDebug() << QString("setscrollbarmaximum GAC = %1").arg(segs->seglgac->NbrOfSegments());
    }
    else if (opts.buttonVIIRSM)
    {
        ui->verticalScrollBar->setMaximum(segs->seglviirsm->NbrOfSegments());
        qDebug() << QString("setscrollbarmaximum VIIRSM = %1").arg(segs->seglviirsm->NbrOfSegments());
    }
    else if (opts.buttonVIIRSDNB)
    {
        ui->verticalScrollBar->setMaximum(segs->seglviirsdnb->NbrOfSegments());
        qDebug() << QString("setscrollbarmaximum VIIRSDNB = %1").arg(segs->seglviirsdnb->NbrOfSegments());
    }
    else if (opts.buttonOLCIefr)
    {
        ui->verticalScrollBar->setMaximum(segs->seglolciefr->NbrOfSegments());
        qDebug() << QString("setscrollbarmaximum OLCIefr = %1").arg(segs->seglolciefr->NbrOfSegments());
    }
    else if (opts.buttonOLCIerr)
    {
        ui->verticalScrollBar->setMaximum(segs->seglolcierr->NbrOfSegments());
        qDebug() << QString("setscrollbarmaximum OLCIerr = %1").arg(segs->seglolcierr->NbrOfSegments());
    }
    else if (opts.buttonRealTime)
    {
        ui->verticalScrollBar->setMaximum(0);
    }

    showSegmentList(0);
}

void FormMapCyl::showSegmentList(int value)
{

    QDateTime first, last;
    QString outp;
    int nbrseg;

    if(opts.buttonMetop)
    {
        segs->seglmetop->ShowSegment(value);
        segs->seglmetop->GetFirstLastVisible(&first, &last);
        nbrseg = segs->seglmetop->NbrOfSegments();

        outp = QString("Metop From %1 to %2  #Segments %3").arg(first.toString(Qt::TextDate)).arg(last.toString(Qt::TextDate)).arg(nbrseg);
    }
    else if(opts.buttonNoaa)
    {
        segs->seglnoaa->ShowSegment(value);
        segs->seglnoaa->GetFirstLastVisible(&first, &last);
        nbrseg = segs->seglnoaa->NbrOfSegments();

        outp = QString("Noaa From %1 to %2  #Segments %3").arg(first.toString(Qt::TextDate)).arg(last.toString(Qt::TextDate)).arg(nbrseg);
    }
    else if(opts.buttonHRP)
    {
        segs->seglhrp->ShowSegment(value);
        segs->seglhrp->GetFirstLastVisible(&first, &last);
        nbrseg = segs->seglhrp->NbrOfSegments();

        outp = QString("HRP From %1 to %2  #Segments %3").arg(first.toString(Qt::TextDate)).arg(last.toString(Qt::TextDate)).arg(nbrseg);
    }
    else if(opts.buttonGAC)
    {
        segs->seglgac->ShowSegment(value);
        segs->seglgac->GetFirstLastVisible(&first, &last);
        nbrseg = segs->seglgac->NbrOfSegments();

        outp = QString("GAC From %1 to %2  #Segments %3").arg(first.toString(Qt::TextDate)).arg(last.toString(Qt::TextDate)).arg(nbrseg);
    }
    else if(opts.buttonVIIRSM)
    {
        segs->seglviirsm->ShowSegment(value);
        segs->seglviirsm->GetFirstLastVisible(&first, &last);
        nbrseg = segs->seglviirsm->NbrOfSegments();

        outp = QString("VIIRSM From %1 to %2  #Segments %3").arg(first.toString(Qt::TextDate)).arg(last.toString(Qt::TextDate)).arg(nbrseg);
    }
    else if(opts.buttonVIIRSDNB)
    {
        segs->seglviirsdnb->ShowSegment(value);
        segs->seglviirsdnb->GetFirstLastVisible(&first, &last);
        nbrseg = segs->seglviirsdnb->NbrOfSegments();

        outp = QString("VIIRSDNB From %1 to %2  #Segments %3").arg(first.toString(Qt::TextDate)).arg(last.toString(Qt::TextDate)).arg(nbrseg);
    }
    else if(opts.buttonOLCIefr)
    {
        segs->seglolciefr->ShowSegment(value);
        segs->seglolciefr->GetFirstLastVisible(&first, &last);
        nbrseg = segs->seglolciefr->NbrOfSegments();

        outp = QString("OLCI EFR From %1 to %2  #Segments %3").arg(first.toString(Qt::TextDate)).arg(last.toString(Qt::TextDate)).arg(nbrseg);
    }
    else if(opts.buttonOLCIerr)
    {
        segs->seglolcierr->ShowSegment(value);
        segs->seglolcierr->GetFirstLastVisible(&first, &last);
        nbrseg = segs->seglolcierr->NbrOfSegments();

        outp = QString("OLCI ERR From %1 to %2  #Segments %3").arg(first.toString(Qt::TextDate)).arg(last.toString(Qt::TextDate)).arg(nbrseg);
    }
    else if(opts.buttonRealTime)
    {
        outp = QString("Real time");
    }

    emit signalSegmentChanged(outp);
    mapcyl->update();

}

void FormMapCyl::RemoveAllSelected()
{
    segs->RemoveAllSelectedAVHRR();
    segs->RemoveAllSelectedVIIRSM();
    segs->RemoveAllSelectedVIIRSDNB();
    segs->RemoveAllSelectedOLCIefr();
    segs->RemoveAllSelectedOLCIerr();

    imageptrs->ptrProjectionBrightnessTemp.reset();
    imageptrs->ptrProjectionInfra.reset();

    mapcyl->update();
    showSegmentcount();

}

void FormMapCyl::slotNothingSelected()
{
    showSegmentcount();
}

void FormMapCyl::on_btnRemoveSelected_clicked()
{
    RemoveAllSelected();
}

void FormMapCyl::on_btnMakeImage_clicked()
{
    if(!formtoolbox->comboColVIIRSOK())
    {
        QMessageBox msgBox;
        msgBox.setText("Need color choices for 3 different bands in the VIIRS tab.");
        //msgBox.setInformativeText("Do you want to save your changes?");
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setIcon(QMessageBox::Warning);
        int ret = msgBox.exec();

        switch (ret) {
        case QMessageBox::Ok:
            break;
        default:
            break;
        }

        return;
    }

    emit emitMakeImage();
}

void FormMapCyl::on_btnSaveTexture_clicked()
{

    QDateTime dt;
    dt = QDateTime::currentDateTime();

    QApplication::setOverrideCursor(Qt::WaitCursor);
    imageptrs->pmOut->save("texture" + dt.toString("yyyyMMddhhmmss") + ".jpg");
    //globe->printTexture();
    qDebug() << "Texture saved to texture" + dt.toString("yyyyMMddhhmmss") + ".jpg";
    QApplication::restoreOverrideCursor();

}

void FormMapCyl::on_btnClearTexture_clicked()
{
    QPixmap pm = (*imageptrs->pmOriginal);
    *imageptrs->pmOut = (*imageptrs->pmOriginal).copy();
    opts.texture_changed = true;
}


void FormMapCyl::on_verticalScrollBar_valueChanged(int value)
{
    showSegmentList(value);
}

void FormMapCyl::on_btnNoaa_clicked()
{
    formtoolbox->setTabWidgetIndex(TAB_AVHRR);
    toggleButton(eSegmentType::SEG_NOAA);
    this->RemoveAllSelected();
    this->setScrollBarMaximum();

}

void FormMapCyl::on_btnMetop_clicked()
{
    formtoolbox->setTabWidgetIndex(TAB_AVHRR);
    toggleButton(eSegmentType::SEG_METOP);
    this->RemoveAllSelected();
    this->setScrollBarMaximum();
}

void FormMapCyl::on_btnHRP_clicked()
{
    formtoolbox->setTabWidgetIndex(TAB_AVHRR);
    toggleButton(eSegmentType::SEG_HRP);
    this->RemoveAllSelected();
    this->setScrollBarMaximum();
}

void FormMapCyl::on_btnGAC_clicked()
{
    formtoolbox->setTabWidgetIndex(TAB_AVHRR);
    toggleButton(eSegmentType::SEG_GAC);
    this->RemoveAllSelected();
    this->setScrollBarMaximum();
}

void FormMapCyl::on_btnRealTime_clicked()
{
    toggleButton(eSegmentType::SEG_NONE);
    this->RemoveAllSelected();
    this->setScrollBarMaximum();

}

void FormMapCyl::on_btnVIIRSM_clicked() // M-Bands
{

    formtoolbox->setTabWidgetIndex(TAB_VIIRS);
    formtoolbox->setTabWidgetVIIRSIndex(0);
    toggleButton(eSegmentType::SEG_VIIRSM);
    segs->RemoveAllSelectedAVHRR();
    mapcyl->update();
    this->showSegmentcount();
    this->setScrollBarMaximum();
    return;
}

void FormMapCyl::on_btnVIIRSDNB_clicked() // DNB Bands
{
    formtoolbox->setTabWidgetIndex(TAB_VIIRS);
    formtoolbox->setTabWidgetVIIRSIndex(1);
    toggleButton(eSegmentType::SEG_VIIRSDNB);
    segs->RemoveAllSelectedAVHRR();
    mapcyl->update();
    this->showSegmentcount();
    this->setScrollBarMaximum();
    return;
}

void FormMapCyl::on_btnOLCIefr_clicked()
{
    formtoolbox->setTabWidgetIndex(TAB_OLCI);
    toggleButton(eSegmentType::SEG_OLCIEFR);
    this->RemoveAllSelected();
    mapcyl->update();
    this->showSegmentcount();
    this->setScrollBarMaximum();

    return;
}



void FormMapCyl::on_btnOLCIerr_clicked()
{
    formtoolbox->setTabWidgetIndex(TAB_OLCI);
    toggleButton(eSegmentType::SEG_OLCIERR);
    this->RemoveAllSelected();
    mapcyl->update();
    this->showSegmentcount();
    this->setScrollBarMaximum();

    return;
}

void FormMapCyl::on_btnAllSegments_clicked()
{
    segs->setShowAllSegments(ui->btnAllSegments->isChecked());
}

void FormMapCyl::on_btnPhong_clicked()
{
    if(opts.bPhongModel)
    {
        ui->btnPhong->setChecked(false);
        opts.bPhongModel = false;
    }
    else
    {
        ui->btnPhong->setChecked(true);
        opts.bPhongModel = true;
    }
}



