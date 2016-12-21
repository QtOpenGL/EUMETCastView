#include "segmentolci.h"
#include "segmentimage.h"
#include "options.h"
#include <QDebug>
#include "archive_entry.h"
#include <netcdf.h>

//using namespace std;
//using namespace netCDF;
//using namespace netCDF::exceptions;

extern Options opts;
extern SegmentImage *imageptrs;

#include <QMutex>
extern QMutex g_mutex;


SegmentOLCI::SegmentOLCI(eSegmentType type, QFile *filesegment, SatelliteList *satl, QObject *parent) :
  Segment(parent)
{

    bool ok;

    satlist = satl;

    fileInfo.setFile(*filesegment);
    if(type == SEG_OLCIEFR)
    {
        segment_type = "OLCIEFR";
        segtype = eSegmentType::SEG_OLCIEFR;
    }
    else if(type == SEG_OLCIERR)
    {
        segment_type = "OLCIERR";
        segtype = eSegmentType::SEG_OLCIERR;
    }

    //0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012
    //0         1         2         3         4         5         6         7         8         9         10
    //S3A_OL_1_EFR____20161026T161414_20161026T161714_20161026T175243_0179_010_168_4319_MAR_O_NR_002.SEN3.tar
    int sensing_start_year = fileInfo.fileName().mid(16, 4).toInt( &ok , 10);
    int sensing_start_month = fileInfo.fileName().mid(20, 2).toInt( &ok, 10);
    int sensing_start_day = fileInfo.fileName().mid(22, 2).toInt( &ok, 10);
    int sensing_start_hour = fileInfo.fileName().mid(25, 2).toInt( &ok, 10);
    int sensing_start_minute = fileInfo.fileName().mid(27, 2).toInt( &ok, 10);
    int sensing_start_second = fileInfo.fileName().mid(29, 2).toInt( &ok, 10);

    int sensing_end_year = fileInfo.fileName().mid(32, 4).toInt( &ok , 10);
    int sensing_end_month = fileInfo.fileName().mid(36, 2).toInt( &ok, 10);
    int sensing_end_day = fileInfo.fileName().mid(38, 2).toInt( &ok, 10);
    int sensing_end_hour = fileInfo.fileName().mid(41, 2).toInt( &ok, 10);
    int sensing_end_minute = fileInfo.fileName().mid(43, 2).toInt( &ok, 10);
    int sensing_end_second = fileInfo.fileName().mid(45, 2).toInt( &ok, 10);

    double d_sensing_start_second = (double)sensing_start_second;
    double d_sensing_end_second = (double)sensing_end_second;

    //this->sensing_start_year = sensing_start_year;
    qdatetime_start.setDate(QDate(sensing_start_year, sensing_start_month, sensing_start_day));
    qdatetime_start.setTime(QTime(sensing_start_hour,sensing_start_minute, sensing_start_second, 0));

    julian_sensing_start = Julian_Date_of_Year(sensing_start_year) +
            DOY( sensing_start_year, sensing_start_month, sensing_start_day ) +
            Fraction_of_Day( sensing_start_hour, sensing_start_minute, d_sensing_start_second )
            + 5.787037e-06; /* Round up to nearest 1 sec */

    julian_sensing_end = Julian_Date_of_Year(sensing_end_year) +
            DOY( sensing_end_year, sensing_end_month, sensing_end_day ) +
            Fraction_of_Day( sensing_end_hour, sensing_end_minute, d_sensing_end_second )
            + 5.787037e-06; /* Round up to nearest 1 sec */


    qsensingstart = QSgp4Date(sensing_start_year, sensing_start_month, sensing_start_day, sensing_start_hour, sensing_start_minute, d_sensing_start_second);
    qsensingend = QSgp4Date(sensing_end_year, sensing_end_month, sensing_end_day, sensing_end_hour, sensing_end_minute, d_sensing_end_second);


    this->earth_views_per_scanline = 4865;
    this->NbrOfLines = 4091;

    Satellite s3a;
    ok = satlist->GetSatellite(41335, &s3a);
    line1 = s3a.line1;
    line2 = s3a.line2;

    //line1 = "1 33591U 09005A   11039.40718334  .00000086  00000-0  72163-4 0  8568";
    //line2 = "2 33591  98.8157 341.8086 0013952 344.4168  15.6572 14.11126791103228";
    double epoch = line1.mid(18,14).toDouble(&ok);
    julian_state_vector = Julian_Date_of_Epoch(epoch);

    qtle.reset(new QTle(s3a.sat_name, line1, line2, QTle::wgs72));
    qsgp4.reset(new QSgp4( *qtle ));


    minutes_since_state_vector = ( julian_sensing_start - julian_state_vector ) * MIN_PER_DAY; //  + (1.0/12.0) / 60.0;
    minutes_sensing = ( julian_sensing_end - julian_sensing_start ) * MIN_PER_DAY;

    QEci qeci;
    qsgp4->getPosition(minutes_since_state_vector, qeci);
    QGeodetic qgeo = qeci.ToGeo();

    lon_start_rad = qgeo.longitude;
    lat_start_rad = qgeo.latitude;

    lon_start_deg = rad2deg(lon_start_rad);
    if (lon_start_deg > 180)
        lon_start_deg = - (360 - rad2deg(lon_start_rad));

    lat_start_deg = rad2deg(lat_start_rad);


    double hours_since_state_vector = ( julian_sensing_start - julian_state_vector ) * HOURS_PER_DAY;

    // qDebug() << QString("---> lon = %1 lat = %2  hours_since_state_vector = %3").arg(lon_start_deg).arg(lat_start_deg).arg( hours_since_state_vector);

    CalculateCornerPoints();
    if(segtype == SEG_OLCIERR)
        CalculateDetailCornerPoints();


    invertthissegment[0] = false;
    invertthissegment[1] = false;
    invertthissegment[2] = false;
    saturationindex[0] = 0;
    saturationindex[1] = 0;
    saturationindex[2] = 0;


}


Segment *SegmentOLCI::ReadSegmentInMemory()
{

    QString fname1;
    QString fname2;
    QString fname3;
    QByteArray array1;
    QByteArray array2;
    QByteArray array3;
    const char* pfile1;
    const char* pfile2;
    const char* pfile3;
    QString var1;
    QString var2;
    QString var3;
    const char* pvar1;
    const char* pvar2;
    const char* pvar3;

    int retval;
    int ncgeofileid, nctiegeofileid, ncfileid1, ncfileid2, ncfileid3;
    int radianceid1, radianceid2, radianceid3;
    int ncqualityflagsid, qualityflagsid;
    float scale_factor[3], add_offset[3];


    int columnsid, rowsid;
    int tiecolumnsid, tierowsid;
    size_t columnslength, rowslength;
    size_t tiecolumnslength, tierowslength;

    int longitudeid, latitudeid;
    int SZAid;
    QScopedArrayPointer<int> tieSZA;
    QScopedArrayPointer<float> secSZA;


    bool iscolorimage = this->bandlist.at(0);

    QString geofile = this->fileInfo.baseName() + ".SEN3/geo_coordinates.nc";
    QByteArray arraygeo = geofile.toUtf8();
    const char *pgeofile = arraygeo.constData();

    qDebug() << "Starting netCDF geo_coordinates";
    retval = nc_open(pgeofile, NC_NOWRITE, &ncgeofileid);
    if(retval != NC_NOERR) qDebug() << "error opening geofile";

    retval = nc_inq_dimid(ncgeofileid, "columns", &columnsid);
    if(retval != NC_NOERR) qDebug() << "error reading columns id geofile";
    retval = nc_inq_dimlen(ncgeofileid, columnsid, &columnslength);
    if(retval != NC_NOERR) qDebug() << "error reading columns length geofile";

    retval = nc_inq_dimid(ncgeofileid, "rows", &rowsid);
    if(retval != NC_NOERR) qDebug() << "error reading rows id geofile";
    retval = nc_inq_dimlen(ncgeofileid, rowsid, &rowslength);
    if(retval != NC_NOERR) qDebug() << "error reading rows length geofile";


    this->longitude.reset(new int[columnslength * rowslength]);
    this->latitude.reset(new int[columnslength * rowslength]);


    retval = nc_inq_varid(ncgeofileid, "longitude", &longitudeid);
    if (retval != NC_NOERR) qDebug() << "error reading longitude id";
    retval = nc_get_var_int(ncgeofileid, longitudeid, this->longitude.data());
    if (retval != NC_NOERR) qDebug() << "error reading longitude values";

    retval = nc_inq_varid(ncgeofileid, "latitude", &latitudeid);
    if (retval != NC_NOERR) qDebug() << "error reading latitude id";
    retval = nc_get_var_int(ncgeofileid, latitudeid, this->latitude.data());
    if (retval != NC_NOERR) qDebug() << "error reading latitude values";

    retval = nc_close(ncgeofileid);
    if (retval != NC_NOERR) qDebug() << "error closing geofile";

//    for(int i = 0; i < columnslength; i++)
//        qDebug() << i << " " << this->longitude[i];

    QString tiegeofile = this->fileInfo.baseName() + ".SEN3/tie_geometries.nc";
    QByteArray arraytiegeo = tiegeofile.toUtf8();
    const char *ptiegeofile = arraytiegeo.constData();

    qDebug() << "Starting netCDF tie_geometries";
    retval = nc_open(ptiegeofile, NC_NOWRITE, &nctiegeofileid);
    if(retval != NC_NOERR) qDebug() << "error opening tie_geometries";

    retval = nc_inq_dimid(nctiegeofileid, "tie_columns", &tiecolumnsid);
    if(retval != NC_NOERR) qDebug() << "error reading tie_columns id";
    retval = nc_inq_dimlen(nctiegeofileid, tiecolumnsid, &tiecolumnslength);
    if(retval != NC_NOERR) qDebug() << "error reading tie_columns length";

    retval = nc_inq_dimid(nctiegeofileid, "tie_rows", &tierowsid);
    if(retval != NC_NOERR) qDebug() << "error reading tie_rows id";
    retval = nc_inq_dimlen(nctiegeofileid, tierowsid, &tierowslength);
    if(retval != NC_NOERR) qDebug() << "error reading tie_rows length";

    secSZA.reset(new float[columnslength * rowslength]);
    tieSZA.reset(new int[tiecolumnslength * tierowslength]);


    retval = nc_inq_varid(nctiegeofileid, "SZA", &SZAid);
    if (retval != NC_NOERR) qDebug() << "error reading SZA id";
    retval = nc_get_var_int(nctiegeofileid, SZAid, tieSZA.data());
    if (retval != NC_NOERR) qDebug() << "error reading SZA values";

    retval = nc_close(nctiegeofileid);
    if (retval != NC_NOERR) qDebug() << "error closing tie_geometries";


//    for(int i = 0; i < tiecolumnslength; i++)
//         qDebug() << i << " " << tieSZA[i];


    QString qualityflagfile = this->fileInfo.baseName() + ".SEN3/qualityFlags.nc";
    QByteArray arrayquality = qualityflagfile.toUtf8();
    const char *pqualityflagfile = arrayquality.constData();

    qDebug() << "Starting netCDF qualityFlags";
    retval = nc_open(pqualityflagfile, NC_NOWRITE, &ncqualityflagsid);
    if(retval != NC_NOERR) qDebug() << "error opening qualitytieFlags";

    retval = nc_inq_dimid(ncqualityflagsid, "columns", &columnsid);
    if(retval != NC_NOERR) qDebug() << "error reading columns id";
    retval = nc_inq_dimlen(ncqualityflagsid, columnsid, &columnslength);
    if(retval != NC_NOERR) qDebug() << "error reading columns length";

    retval = nc_inq_dimid(ncqualityflagsid, "rows", &rowsid);
    if(retval != NC_NOERR) qDebug() << "error reading rows id";
    retval = nc_inq_dimlen(ncqualityflagsid, rowsid, &rowslength);
    if(retval != NC_NOERR) qDebug() << "error reading rows length";

    quality_flags.reset(new quint32[columnslength * rowslength]);

    retval = nc_inq_varid(ncqualityflagsid, "quality_flags", &qualityflagsid);
    if (retval != NC_NOERR) qDebug() << "error reading qualityflags id";
    retval = nc_get_var_uint(ncqualityflagsid, qualityflagsid, quality_flags.data());
    if (retval != NC_NOERR) qDebug() << "error reading quality_flags values";

    size_t attlen = 0;
    retval = nc_inq_attlen(ncqualityflagsid, qualityflagsid, "flag_meanings", &attlen);
    if (retval != NC_NOERR) qDebug() << "error reading att len flag_meanings";

    qDebug() << "attlen = " << attlen;

    char *string_attr = NULL;
    string_attr = new char[attlen];

    retval = nc_get_att_text(ncqualityflagsid, qualityflagsid, "flag_meanings", string_attr);
    if (retval != NC_NOERR) qDebug() << "error reading att string flag_meanings retval = " << retval ;

    QString strflagmeanings(string_attr);
    this->strlflagmeanings = strflagmeanings.split(" ",QString::SkipEmptyParts);
    for (int i = 0; i < strlflagmeanings.size(); i++)
    {
        qDebug() << i << " " << strlflagmeanings.at(i);
    }

    size_t flagmaskslen;
    retval = nc_inq_attlen(ncqualityflagsid, qualityflagsid, "flag_masks", &flagmaskslen);
    if (retval != NC_NOERR) qDebug() << "error reading att flag masks len retval = " << retval ;

    masks.reset(new quint32[flagmaskslen]);

    retval = nc_get_att_uint(ncqualityflagsid, qualityflagsid, "flag_masks", masks.data());
    if (retval != NC_NOERR) qDebug() << "error reading att flag_masks retval = " << retval ;

    for (int i = 0; i < flagmaskslen; i++)
    {
        qDebug() << i << " " << masks[i];
    }

    nbrsaturatedpixels = 0;
    nbrcoastlinepixels = 0;
    for (int line = 0; line < this->NbrOfLines; line++)
    {
        for (int pixelx = 0; pixelx < earth_views_per_scanline; pixelx++)
        {
            if(0x001fffff & quality_flags[line * earth_views_per_scanline + pixelx])
            {
                saturatedpixels << QPoint(pixelx, line);
                nbrsaturatedpixels++;
            }
            if(1073741824 & quality_flags[line * earth_views_per_scanline + pixelx])
            {
                coastline << QPoint(pixelx, line);
                nbrcoastlinepixels++;
            }

        }
    }

    qDebug() << "Nbr of saturated pixels = " << nbrsaturatedpixels << " nbr of coastline pixels = " << nbrcoastlinepixels;



    delete [] string_attr;
    retval = nc_close(ncqualityflagsid);
    if (retval != NC_NOERR) qDebug() << "error closing qualityFlags";




    if(iscolorimage)
    {
        qDebug() << "Starting netCDF color";
        getDatasetNameFromColor(0, &fname1, &var1, &saturationindex[0]);
        getDatasetNameFromColor(1, &fname2, &var2, &saturationindex[1]);
        getDatasetNameFromColor(2, &fname3, &var3, &saturationindex[2]);

        qDebug() << "getDatasetNameFromBand fname1 = " << fname1 << " var1 = " << var1;
        qDebug() << "getDatasetNameFromBand fname2 = " << fname2 << " var2 = " << var2;
        qDebug() << "getDatasetNameFromBand fname3 = " << fname3 << " var3 = " << var3;


        array1 = fname1.toUtf8();
        array2 = fname2.toUtf8();
        array3 = fname3.toUtf8();
        pfile1 = array1.constData();
        pfile2 = array2.constData();
        pfile3 = array3.constData();

        retval = nc_open(pfile1, NC_NOWRITE, &ncfileid1);
        if(retval != NC_NOERR) qDebug() << "error opening file1";
        retval = nc_open(pfile2, NC_NOWRITE, &ncfileid2);
        if(retval != NC_NOERR) qDebug() << "error opening file2";
        retval = nc_open(pfile3, NC_NOWRITE, &ncfileid3);
        if(retval != NC_NOERR) qDebug() << "error opening file3";

        retval = nc_inq_dimid(ncfileid1, "columns", &columnsid);
        if(retval != NC_NOERR) qDebug() << "error reading columns id file1";
        retval = nc_inq_dimlen(ncfileid1, columnsid, &columnslength);
        if(retval != NC_NOERR) qDebug() << "error reading columns length file1";

        retval = nc_inq_dimid(ncfileid1, "rows", &rowsid);
        if(retval != NC_NOERR) qDebug() << "error reading rows id file1";
        retval = nc_inq_dimlen(ncfileid1, rowsid, &rowslength);
        if(retval != NC_NOERR) qDebug() << "error reading rows length file1";

        this->earth_views_per_scanline = columnslength;
        this->NbrOfLines = rowslength;

        this->initializeMemory();

        array1 = var1.toUtf8();
        array2 = var2.toUtf8();
        array3 = var3.toUtf8();
        pvar1 = array1.constData();
        pvar2 = array2.constData();
        pvar3 = array3.constData();

        retval = nc_inq_varid(ncfileid1, pvar1, &radianceid1);
        if (retval != NC_NOERR) qDebug() << "error reading radiance1 id";
        retval = nc_get_var_ushort(ncfileid1, radianceid1, ptrbaOLCI[0].data());
        if (retval != NC_NOERR) qDebug() << "error reading radiance1 values";

        retval = nc_inq_varid(ncfileid2, pvar2, &radianceid2);
        if (retval != NC_NOERR) qDebug() << "error reading radiance2 id";
        retval = nc_get_var_ushort(ncfileid2, radianceid2, ptrbaOLCI[1].data());
        if (retval != NC_NOERR) qDebug() << "error reading radiance2 values";

        retval = nc_inq_varid(ncfileid3, pvar3, &radianceid3);
        if (retval != NC_NOERR) qDebug() << "error reading radiance3 id";
        retval = nc_get_var_ushort(ncfileid3, radianceid3, ptrbaOLCI[2].data());
        if (retval != NC_NOERR) qDebug() << "error reading radiance3 values";

        retval = nc_get_att_float(ncfileid1, radianceid1, "scale_factor", &scale_factor[0]);
        if (retval != NC_NOERR) qDebug() << "error reading scale_factor[0]";

        retval = nc_get_att_float(ncfileid2, radianceid2, "scale_factor", &scale_factor[1]);
        if (retval != NC_NOERR) qDebug() << "error reading scale_factor[1]";

        retval = nc_get_att_float(ncfileid3, radianceid3, "scale_factor", &scale_factor[2]);
        if (retval != NC_NOERR) qDebug() << "error reading scale_factor[2]";

        retval = nc_get_att_float(ncfileid1, radianceid1, "add_offset", &add_offset[0]);
        if (retval != NC_NOERR) qDebug() << "error reading add_offset[0]";

        retval = nc_get_att_float(ncfileid2, radianceid2, "add_offset", &add_offset[1]);
        if (retval != NC_NOERR) qDebug() << "error reading add_offset[1]";

        retval = nc_get_att_float(ncfileid3, radianceid3, "add_offset", &add_offset[2]);
        if (retval != NC_NOERR) qDebug() << "error reading add_offset[2]";


        for(int i = 0; i < 3; i++)
            qDebug() << QString("scale_factor %1 = %2").arg(i).arg(scale_factor[i]);

        retval = nc_close(ncfileid1);
        if (retval != NC_NOERR) qDebug() << "error closing file1";

        retval = nc_close(ncfileid2);
        if (retval != NC_NOERR) qDebug() << "error closing file2";

        retval = nc_close(ncfileid3);
        if (retval != NC_NOERR) qDebug() << "error closing file3";


    }
    else
    {
        qDebug() << "Starting netCDF mono";
        getDatasetNameFromBand(&fname1, &var1, &saturationindex[0]);

        qDebug() << "getDatasetNameFromBand fname1 = " << fname1 << " var1 = " << var1;

        array1 = fname1.toUtf8();
        pfile1 = array1.constData();

        retval = nc_open(pfile1, NC_NOWRITE, &ncfileid1);
        if(retval != NC_NOERR) qDebug() << "error opening file1";

        retval = nc_inq_dimid(ncfileid1, "columns", &columnsid);
        if(retval != NC_NOERR) qDebug() << "error reading columns id file1";
        retval = nc_inq_dimlen(ncfileid1, columnsid, &columnslength);
        if(retval != NC_NOERR) qDebug() << "error reading columns length file1";

        retval = nc_inq_dimid(ncfileid1, "rows", &rowsid);
        if(retval != NC_NOERR) qDebug() << "error reading rows id file1";
        retval = nc_inq_dimlen(ncfileid1, rowsid, &rowslength);
        if(retval != NC_NOERR) qDebug() << "error reading rows length file1";

        this->earth_views_per_scanline = columnslength;
        this->NbrOfLines = rowslength;

        this->initializeMemory();

        array1 = var1.toUtf8();
        pvar1 = array1.constData();

        retval = nc_inq_varid(ncfileid1, pvar1, &radianceid1);
        if (retval != NC_NOERR) qDebug() << "error reading radiance1 id";
        retval = nc_get_var_ushort(ncfileid1, radianceid1, ptrbaOLCI[0].data());
        if (retval != NC_NOERR) qDebug() << "error reading radiance1 values";

        retval = nc_get_att_float(ncfileid1, radianceid1, "scale_factor", &scale_factor[0]);
        if (retval != NC_NOERR) qDebug() << "error reading scale_factor[0]";

        retval = nc_get_att_float(ncfileid1, radianceid1, "add_offset", &add_offset[0]);
        if (retval != NC_NOERR) qDebug() << "error reading add_offset[0]";

        retval = nc_close(ncfileid1);
        if (retval != NC_NOERR) qDebug() << "error closing file1";


    }

    int val1, val2, diff;

    for(int k = 0; k < 3; k++)
    {
        stat_max_ch[k] = 0;
        stat_min_ch[k] = 9999999;
        stat_max_norm_ch[k] = 0;
        stat_min_norm_ch[k] = 9999999;
        active_pixels[k] = 0;
    }

    qDebug() << QString("rowslength = %1 columnslength : %2 earth_views_per_scanline = %3").arg(rowslength).arg(columnslength).arg(earth_views_per_scanline);
    qDebug() << QString("tierowslength = %1 tiecolumnslength : %2 NbrOfLines = %3").arg(tierowslength).arg(tiecolumnslength).arg(NbrOfLines);

    int factor = (columnslength-1)/(tiecolumnslength-1);

    qDebug() << QString("rowslength * columnslength = %1 factor = %2 ").arg(rowslength*columnslength).arg(factor);

    // Linear interpolation
    for(int j=0; j < NbrOfLines; j++)
    {
        for(int i=0; i < tiecolumnslength-1; i++) // tiecolumnslength = 77
        {
            val1 = tieSZA[j*tiecolumnslength + i];
            val2 = tieSZA[j*tiecolumnslength + i+1];
            diff = (val2 - val1)/factor;

            for(int k=0; k < factor; k++)
            {
               secSZA[j*earth_views_per_scanline + i*factor + k] = 1.0/cos(((float)(val1 + diff*k)/1000000.0f)*PI/180.0);
            }
        }
        secSZA[j*earth_views_per_scanline + 76*factor] = 1.0/cos(((float)val2/1000000.0f)*PI/180.0);
    }

    float val, valnormalized;

    for(int j=0; j < NbrOfLines; j++)
    {
        for(int i=0; i < earth_views_per_scanline; i++)
        {
            for(int k = 0; k < (iscolorimage ? 3 : 1); k++)
            {
                if(ptrbaOLCI[k][j*earth_views_per_scanline + i] < 65535)
                {
                    val = ((float)ptrbaOLCI[k][j*earth_views_per_scanline + i] * scale_factor[k] + add_offset[k]) * 10;
                    valnormalized = val*secSZA[j*earth_views_per_scanline + i];
                    ptrbaOLCI[k][j*earth_views_per_scanline + i]  = (quint16)qMin(qMax(qRound(val),0),65535);
                    ptrbaOLCInormalized[k][j*earth_views_per_scanline + i]  = (quint16)qMin(qMax(qRound(valnormalized),0),65535);
                }
                else
                {
                    ptrbaOLCI[k][j*earth_views_per_scanline + i] = 65535;
                    ptrbaOLCInormalized[k][j*earth_views_per_scanline + i] = 65535;
                }

            }
        }
    }

    for(int k = 0; k < (iscolorimage ? 3 : 1); k++)
    {
        for(int j=0; j < NbrOfLines; j++)
        {
            for(int i=0; i < earth_views_per_scanline; i++)
            {
                if(ptrbaOLCI[k][j*earth_views_per_scanline + i] < 65535)
                {
                    if(ptrbaOLCI[k][j*earth_views_per_scanline + i] > stat_max_ch[k])
                        stat_max_ch[k] = ptrbaOLCI[k][j*earth_views_per_scanline + i];
                    if(ptrbaOLCI[k][j*earth_views_per_scanline + i] < stat_min_ch[k])
                        stat_min_ch[k] = ptrbaOLCI[k][j*earth_views_per_scanline + i];
                    if(ptrbaOLCInormalized[k][j*earth_views_per_scanline + i] > stat_max_norm_ch[k])
                        stat_max_norm_ch[k] = ptrbaOLCInormalized[k][j*earth_views_per_scanline + i];
                    if(ptrbaOLCInormalized[k][j*earth_views_per_scanline + i] < stat_min_norm_ch[k])
                        stat_min_norm_ch[k] = ptrbaOLCInormalized[k][j*earth_views_per_scanline + i];

                    active_pixels[k]++;
                }

            }
        }
    }


    qDebug() << QString("ptrbaOLCI min_ch[0] = %1 max_ch[0] = %2").arg(stat_min_ch[0]).arg(stat_max_ch[0]);
    if(iscolorimage)
    {
        qDebug() << QString("ptrbaOLCI min_ch[1] = %1 max_ch[1] = %2").arg(stat_min_ch[1]).arg(stat_max_ch[1]);
        qDebug() << QString("ptrbaOLCI min_ch[2] = %1 max_ch[2] = %2").arg(stat_min_ch[2]).arg(stat_max_ch[2]);

    }
    qDebug() << QString("ptrbaOLCInormalized min_ch[0] = %1 max_ch[0] = %2").arg(stat_min_norm_ch[0]).arg(stat_max_norm_ch[0]);
    if(iscolorimage)
    {
        qDebug() << QString("ptrbaOLCInormalized min_ch[1] = %1 max_ch[1] = %2").arg(stat_min_norm_ch[1]).arg(stat_max_norm_ch[1]);
        qDebug() << QString("ptrbaOLCInormalized min_ch[2] = %1 max_ch[2] = %2").arg(stat_min_norm_ch[2]).arg(stat_max_norm_ch[2]);
    }


    return this;

}

// There is no difference between a linear interpolation and the Lagrange interpolation
float SegmentOLCI::getSolarZenith(int *tieSZA, int navpoint, int intpoint, int nbrLine) //navpoint = [0, 76] intpoint = [0, 63] nbrLine = [0, this->NbrOfLines]
{
    // second order Lagrange interpolation ==> 3 points
    //  from pt 0 --> pt 4864
    //    = 64 * 76 = 4864

    float a, k, s, t;
    float x[3];
    float y[3];

    int n = 3;
    if(navpoint == 0)
    {
        y[0] = (float)tieSZA[nbrLine*77];
        y[1] = (float)tieSZA[nbrLine*77 + 1];
        y[2] = (float)tieSZA[nbrLine*77 + 2];
        x[0] = 0.0f;
        x[1] = 64.0f;
        x[2] = 128.0f;
    }
    else
    {
        y[0] = (float)tieSZA[nbrLine*77 + navpoint - 1];
        y[1] = (float)tieSZA[nbrLine*77 + navpoint];
        y[2] = (float)tieSZA[nbrLine*77 + navpoint + 1];
        x[0] = (float)(navpoint-1) * 64;
        x[1] = (float)navpoint * 64;
        x[2] = (float)(navpoint+1) * 64;
    }

    k = 0;
    a = navpoint * 64 + intpoint;

    for(int i=0; i<n; i++)
    {
        s=1;
        t=1;
        for(int j=0; j<n; j++)
        {
            if(j!=i)
            {
                s=s*(a-x[j]);
                t=t*(x[i]-x[j]);
            }
        }
        k=k+((s/t)*y[i]);
    }
    return k;

}

//Segment *SegmentOLCI::ReadSegmentInMemory()
//{

//    QString fname1;
//    QString fname2;
//    QString fname3;
//    QByteArray array1;
//    QByteArray array2;
//    QByteArray array3;
//    const char* pfname1;
//    const char* pfname2;
//    const char* pfname3;
//    QString var1;
//    QString var2;
//    QString var3;
//    const char* pvar1;
//    const char* pvar2;
//    const char* pvar3;
//    NcVar radiance1;
//    NcVar radiance2;
//    NcVar radiance3;

//    bool iscolorimage = this->bandlist.at(0);


//    qDebug() << "Starting netCDF geo_coordinates";


//    QString geofile = this->fileInfo.baseName() + ".SEN3/geo_coordinates.nc";
//    QByteArray arraygeo = geofile.toUtf8();
//    const char *pgeofile = arraygeo.constData();
//    NcFile geoFile(pgeofile, NcFile::read);
//    if(geoFile.isNull()) qDebug() << "error opening geoFile";


//    NcDim coldim = geoFile.getDim("columns");
//    NcDim rowdim = geoFile.getDim("rows");
//    int columns = coldim.getSize();
//    int rows = rowdim.getSize();


//    this->longitude.reset(new int[columns * rows]);
//    this->latitude.reset(new int[columns * rows]);

//    NcVar geolon = geoFile.getVar("longitude");
//    if(geolon.isNull()) qDebug() << "error getVar geolon";
//    geolon.getVar(this->longitude.data());

//    NcVar geolat = geoFile.getVar("latitude");
//    if(geolat.isNull()) qDebug() << "error getVar geolat";
//    geolat.getVar(this->latitude.data());

//    if(iscolorimage)
//    {
//        qDebug() << "Starting netCDF color";
//        getDatasetNameFromColor(0, &fname1, &var1);
//        getDatasetNameFromColor(1, &fname2, &var2);
//        getDatasetNameFromColor(2, &fname3, &var3);

//        qDebug() << "getDatasetNameFromBand fname1 = " << fname1 << " var1 = " << var1;
//        qDebug() << "getDatasetNameFromBand fname2 = " << fname2 << " var2 = " << var2;
//        qDebug() << "getDatasetNameFromBand fname3 = " << fname3 << " var3 = " << var3;


//        array1 = fname1.toUtf8();
//        array2 = fname2.toUtf8();
//        array3 = fname3.toUtf8();
//        pfname1 = array1.constData();
//        pfname2 = array2.constData();
//        pfname3 = array3.constData();

//        NcFile dataFile1(pfname1, NcFile::read);
//        if(dataFile1.isNull()) qDebug() << "error opening dataFile1";

//        NcFile dataFile2(pfname2, NcFile::read);
//        if(dataFile2.isNull()) qDebug() << "error opening dataFile2";

//        NcFile dataFile3(pfname3, NcFile::read);
//        if(dataFile3.isNull()) qDebug() << "error opening dataFile3";


//        NcDim coldim = dataFile1.getDim("columns");
//        NcDim rowdim = dataFile1.getDim("rows");
//        this->earth_views_per_scanline = coldim.getSize();
//        this->NbrOfLines = rowdim.getSize();


//        this->initializeMemory();

//        for(int k = 0; k < 3; k++)
//        {
//            stat_max_ch[k] = 0;
//            stat_min_ch[k] = 9999999;
//            active_pixels[k] = 0;
//        }


//        array1 = var1.toUtf8();
//        array2 = var2.toUtf8();
//        array3 = var3.toUtf8();
//        pvar1 = array1.constData();
//        pvar2 = array2.constData();
//        pvar3 = array3.constData();


//        radiance1 = dataFile1.getVar(pvar1);
//        if(radiance1.isNull()) qDebug() << "error getVar radiance1";
//        radiance1.getVar(ptrbaOLCI[0].data());

//        radiance2 = dataFile2.getVar(pvar2);
//        if(radiance2.isNull()) qDebug() << "error getVar radiance2";
//        radiance2.getVar(ptrbaOLCI[1].data());

//        radiance3 = dataFile3.getVar(pvar3);
//        if(radiance3.isNull()) qDebug() << "error getVar radiance3";
//        radiance3.getVar(ptrbaOLCI[2].data());

//    }
//    else
//    {
//        qDebug() << "Starting netCDF mono";

//        getDatasetNameFromBand(&fname1, &var1);
//        qDebug() << "getDatasetNameFromBand fname1 = " << fname1 << " var1 = " << var1;
//        array1 = fname1.toUtf8();
//        pfname1 = array1.constData();
//        NcFile dataFile1(pfname1, NcFile::read);
//        if(dataFile1.isNull()) qDebug() << "error opening dataFile1";


//        NcDim coldim = dataFile1.getDim("columns");
//        NcDim rowdim = dataFile1.getDim("rows");
//        this->earth_views_per_scanline = coldim.getSize();
//        this->NbrOfLines = rowdim.getSize();

//        qDebug() << QString("num_dims = %1 num_vars = %2 num_atts = %3 group_dim = %4")
//                            .arg(dataFile1.getDimCount())
//                            .arg(dataFile1.getVarCount())
//                            .arg(dataFile1.getAttCount())
//                            .arg(dataFile1.getGroupCount());

//        this->initializeMemory();
//        for(int k = 0; k < 3; k++)
//        {
//            stat_max_ch[k] = 0;
//            stat_min_ch[k] = 9999999;
//            active_pixels[k] = 0;
//        }

//        array1 = var1.toUtf8();
//        pvar1 = array1.constData();

//        radiance1 = dataFile1.getVar(pvar1);
//        if(radiance1.isNull()) qDebug() << "error getVar radiance1";
//        radiance1.getVar(ptrbaOLCI[0].data());

//    }



//    for(int k = 0; k < (iscolorimage ? 3 : 1); k++)
//    {
//        for(int j=0; j < NbrOfLines; j++)
//        {
//            for(int i=0; i < earth_views_per_scanline; i++)
//            {
//                if(ptrbaOLCI[k][j*earth_views_per_scanline + i] < 65535)
//                {
//                    if(ptrbaOLCI[k][j*earth_views_per_scanline + i] > stat_max_ch[k])
//                        stat_max_ch[k] = ptrbaOLCI[k][j*earth_views_per_scanline + i];
//                    if(ptrbaOLCI[k][j*earth_views_per_scanline + i] < stat_min_ch[k])
//                        stat_min_ch[k] = ptrbaOLCI[k][j*earth_views_per_scanline + i];
//                    active_pixels[k]++;
//                }

//            }
//        }
//    }

//    qDebug() << QString("ptrbaOLCI min_ch[0] = %1 max_ch[0] = %2").arg(stat_min_ch[0]).arg(stat_max_ch[0]);
//    if(iscolorimage)
//    {
//        qDebug() << QString("ptrbaOLCI min_ch[1] = %1 max_ch[1] = %2").arg(stat_min_ch[1]).arg(stat_max_ch[1]);
//        qDebug() << QString("ptrbaOLCI min_ch[2] = %1 max_ch[2] = %2").arg(stat_min_ch[2]).arg(stat_max_ch[2]);

//    }


//}

void SegmentOLCI::getDatasetNameFromColor(int colorindex, QString *datasetname, QString *variablename, int *saturationindex)
{
    qDebug() << "getDatasetNameFromColor colorindex = " << colorindex;

    Q_ASSERT(colorindex >=0 && colorindex < 3);
    colorindex++; // 1, 2 or 3

    if(colorlist.at(0) == colorindex)
    {
        invertthissegment[colorindex-1] = invertlist.at(0);
        *datasetname = this->fileInfo.baseName() + ".SEN3/Oa01_radiance.nc";
        *variablename = "Oa01_radiance";
        *saturationindex = 11;
    }
    else if(colorlist.at(1) == colorindex)
    {
        invertthissegment[colorindex-1] = invertlist.at(1);
        *datasetname = this->fileInfo.baseName() + ".SEN3/Oa02_radiance.nc";
        *variablename = "Oa02_radiance";
        *saturationindex = 12;
    }
    else if(colorlist.at(2) == colorindex)
    {
        invertthissegment[colorindex-1] = invertlist.at(2);
        *datasetname = this->fileInfo.baseName() + ".SEN3/Oa03_radiance.nc";
        *variablename = "Oa03_radiance";
        *saturationindex = 13;
    }
    else if(colorlist.at(3) == colorindex)
    {
        invertthissegment[colorindex-1] = invertlist.at(3);
        *datasetname = this->fileInfo.baseName() + ".SEN3/Oa04_radiance.nc";
        *variablename = "Oa04_radiance";
        *saturationindex = 14;
    }
    else if(colorlist.at(4) == colorindex)
    {
        invertthissegment[colorindex-1] = invertlist.at(4);
        *datasetname = this->fileInfo.baseName() + ".SEN3/Oa05_radiance.nc";
        *variablename = "Oa05_radiance";
        *saturationindex = 15;
    }
    else if(colorlist.at(5) == colorindex)
    {
        invertthissegment[colorindex-1] = invertlist.at(5);
        *datasetname = this->fileInfo.baseName() + ".SEN3/Oa06_radiance.nc";
        *variablename = "Oa06_radiance";
        *saturationindex = 16;
    }
    else if(colorlist.at(6) == colorindex)
    {
        invertthissegment[colorindex-1] = invertlist.at(6);
        *datasetname = this->fileInfo.baseName() + ".SEN3/Oa07_radiance.nc";
        *variablename = "Oa07_radiance";
        *saturationindex = 17;
    }
    else if(colorlist.at(7) == colorindex)
    {
        invertthissegment[colorindex-1] = invertlist.at(7);
        *datasetname = this->fileInfo.baseName() + ".SEN3/Oa08_radiance.nc";
        *variablename = "Oa08_radiance";
        *saturationindex = 18;
    }
    else if(colorlist.at(8) == colorindex)
    {
        invertthissegment[colorindex-1] = invertlist.at(8);
        *datasetname = this->fileInfo.baseName() + ".SEN3/Oa09_radiance.nc";
        *variablename = "Oa09_radiance";
        *saturationindex = 19;
    }
    else if(colorlist.at(9) == colorindex)
    {
        invertthissegment[colorindex-1] = invertlist.at(9);
        *datasetname = this->fileInfo.baseName() + ".SEN3/Oa10_radiance.nc";
        *variablename = "Oa10_radiance";
        *saturationindex = 20;
   }
    else if(colorlist.at(10) == colorindex)
    {
        invertthissegment[colorindex-1] = invertlist.at(10);
        *datasetname = this->fileInfo.baseName() + ".SEN3/Oa11_radiance.nc";
        *variablename = "Oa11_radiance";
        *saturationindex = 21;
    }
    else if(colorlist.at(11) == colorindex)
    {
        invertthissegment[colorindex-1] = invertlist.at(11);
        *datasetname = this->fileInfo.baseName() + ".SEN3/Oa12_radiance.nc";
        *variablename = "Oa12_radiance";
        *saturationindex = 22;
    }
    else if(colorlist.at(12) == colorindex)
    {
        invertthissegment[colorindex-1] = invertlist.at(12);
        *datasetname = this->fileInfo.baseName() + ".SEN3/Oa13_radiance.nc";
        *variablename = "Oa13_radiance";
        *saturationindex = 23;
    }
    else if(colorlist.at(13) == colorindex)
    {
        invertthissegment[colorindex-1] = invertlist.at(13);
        *datasetname = this->fileInfo.baseName() + ".SEN3/Oa14_radiance.nc";
        *variablename = "Oa14_radiance";
        *saturationindex = 24;
    }
    else if(colorlist.at(14) == colorindex)
    {
        invertthissegment[colorindex-1] = invertlist.at(14);
        *datasetname = this->fileInfo.baseName() + ".SEN3/Oa15_radiance.nc";
        *variablename = "Oa15_radiance";
        *saturationindex = 25;
    }
    else if(colorlist.at(15) == colorindex)
    {
        invertthissegment[colorindex-1] = invertlist.at(15);
        *datasetname = this->fileInfo.baseName() + ".SEN3/Oa16_radiance.nc";
        *variablename = "Oa16_radiance";
        *saturationindex = 26;
    }
    else if(colorlist.at(16) == colorindex)
    {
        invertthissegment[colorindex-1] = invertlist.at(16);
        *datasetname = this->fileInfo.baseName() + ".SEN3/Oa17_radiance.nc";
        *variablename = "Oa17_radiance";
        *saturationindex = 27;
    }
    else if(colorlist.at(17) == colorindex)
    {
        invertthissegment[colorindex-1] = invertlist.at(17);
        *datasetname = this->fileInfo.baseName() + ".SEN3/Oa18_radiance.nc";
        *variablename = "Oa18_radiance";
        *saturationindex = 28;
    }
    else if(colorlist.at(18) == colorindex)
    {
        invertthissegment[colorindex-1] = invertlist.at(18);
        *datasetname = this->fileInfo.baseName() + ".SEN3/Oa19_radiance.nc";
        *variablename = "Oa19_radiance";
        *saturationindex = 29;
    }
    else if(colorlist.at(19) == colorindex)
    {
        invertthissegment[colorindex-1] = invertlist.at(19);
        *datasetname = this->fileInfo.baseName() + ".SEN3/Oa20_radiance.nc";
        *variablename = "Oa20_radiance";
        *saturationindex = 30;
    }
    else if(colorlist.at(20) == colorindex)
    {
        invertthissegment[colorindex-1] = invertlist.at(20);
        *datasetname = this->fileInfo.baseName() + ".SEN3/Oa21_radiance.nc";
        *variablename = "Oa21_radiance";
        *saturationindex = 31;
    }
}

void SegmentOLCI::getDatasetNameFromBand(QString *datasetname, QString *variablename, int *saturationindex)
{
    if(bandlist.at(1))
    {
        invertthissegment[0] = invertlist.at(0);
        *datasetname = this->fileInfo.baseName() + ".SEN3/Oa01_radiance.nc";
        *variablename = "Oa01_radiance";
        *saturationindex = 11;
    }
    else if(bandlist.at(2))
    {
        invertthissegment[0] = invertlist.at(1);
        *datasetname = this->fileInfo.baseName() + ".SEN3/Oa02_radiance.nc";
        *variablename = "Oa02_radiance";
        *saturationindex = 12;
    }
    else if(bandlist.at(3))
    {
        invertthissegment[0] = invertlist.at(2);
        *datasetname = this->fileInfo.baseName() + ".SEN3/Oa03_radiance.nc";
        *variablename = "Oa03_radiance";
        *saturationindex = 13;
    }
    else if(bandlist.at(4))
    {
        invertthissegment[0] = invertlist.at(3);
        *datasetname = this->fileInfo.baseName() + ".SEN3/Oa04_radiance.nc";
        *variablename = "Oa04_radiance";
        *saturationindex = 14;
    }
    else if(bandlist.at(5))
    {
        invertthissegment[0] = invertlist.at(4);
        *datasetname = this->fileInfo.baseName() + ".SEN3/Oa05_radiance.nc";
        *variablename = "Oa05_radiance";
        *saturationindex = 15;
    }
    else if(bandlist.at(6))
    {
       invertthissegment[0] = invertlist.at(5);
        *datasetname = this->fileInfo.baseName() + ".SEN3/Oa06_radiance.nc";
        *variablename = "Oa06_radiance";
       *saturationindex = 16;
    }
    else if(bandlist.at(7))
    {
        invertthissegment[0] = invertlist.at(6);
        *datasetname = this->fileInfo.baseName() + ".SEN3/Oa07_radiance.nc";
        *variablename = "Oa07_radiance";
        *saturationindex = 17;
    }
    else if(bandlist.at(8))
    {
        invertthissegment[0] = invertlist.at(7);
        *datasetname = this->fileInfo.baseName() + ".SEN3/Oa08_radiance.nc";
        *variablename = "Oa08_radiance";
        *saturationindex = 18;
    }
    else if(bandlist.at(9))
    {
        invertthissegment[0] = invertlist.at(8);
        *datasetname = this->fileInfo.baseName() + ".SEN3/Oa09_radiance.nc";
        *variablename = "Oa09_radiance";
        *saturationindex = 19;
    }
    else if(bandlist.at(10))
    {
        invertthissegment[0] = invertlist.at(9);
        *datasetname = this->fileInfo.baseName() + ".SEN3/Oa10_radiance.nc";
        *variablename = "Oa10_radiance";
        *saturationindex = 20;
    }
    else if(bandlist.at(11))
    {
        invertthissegment[0] = invertlist.at(10);
        *datasetname = this->fileInfo.baseName() + ".SEN3/Oa11_radiance.nc";
        *variablename = "Oa11_radiance";
        *saturationindex = 21;
    }
    else if(bandlist.at(12))
    {
        invertthissegment[0] = invertlist.at(11);
        *datasetname = this->fileInfo.baseName() + ".SEN3/Oa12_radiance.nc";
        *variablename = "Oa12_radiance";
        *saturationindex = 22;
    }
    else if(bandlist.at(13))
    {
        invertthissegment[0] = invertlist.at(12);
        *datasetname = this->fileInfo.baseName() + ".SEN3/Oa13_radiance.nc";
        *variablename = "Oa13_radiance";
        *saturationindex = 23;
    }
    else if(bandlist.at(14))
    {
        invertthissegment[0] = invertlist.at(13);
        *datasetname = this->fileInfo.baseName() + ".SEN3/Oa14_radiance.nc";
        *variablename = "Oa14_radiance";
        *saturationindex = 24;
    }
    else if(bandlist.at(15))
    {
        invertthissegment[0] = invertlist.at(14);
        *datasetname = this->fileInfo.baseName() + ".SEN3/Oa15_radiance.nc";
        *variablename = "Oa15_radiance";
        *saturationindex = 25;
    }
    else if(bandlist.at(16))
    {
        invertthissegment[0] = invertlist.at(15);
        *datasetname = this->fileInfo.baseName() + ".SEN3/Oa16_radiance.nc";
        *variablename = "Oa16_radiance";
        *saturationindex = 26;
    }
    else if(bandlist.at(17))
    {
        invertthissegment[0] = invertlist.at(16);
        *datasetname = this->fileInfo.baseName() + ".SEN3/Oa17_radiance.nc";
        *variablename = "Oa17_radiance";
        *saturationindex = 27;
    }
    else if(bandlist.at(18))
    {
        invertthissegment[0] = invertlist.at(17);
        *datasetname = this->fileInfo.baseName() + ".SEN3/Oa18_radiance.nc";
        *variablename = "Oa18_radiance";
        *saturationindex = 28;
    }
    else if(bandlist.at(19))
    {
        invertthissegment[0] = invertlist.at(18);
        *datasetname = this->fileInfo.baseName() + ".SEN3/Oa19_radiance.nc";
        *variablename = "Oa19_radiance";
        *saturationindex = 29;
    }
    else if(bandlist.at(20))
    {
        invertthissegment[0] = invertlist.at(19);
        *datasetname = this->fileInfo.baseName() + ".SEN3/Oa20_radiance.nc";
        *variablename = "Oa20_radiance";
        *saturationindex = 30;
    }
    else if(bandlist.at(21))
    {
        invertthissegment[0] = invertlist.at(20);
        *datasetname = this->fileInfo.baseName() + ".SEN3/Oa21_radiance.nc";
        *variablename = "Oa21_radiance";
        *saturationindex = 31;
    }

}


void SegmentOLCI::RenderSegmentlineInTextureOLCI( int nbrLine, QRgb *row )
{

    QColor rgb;
    int posx, posy;

    g_mutex.lock();

    QPainter fb_painter(imageptrs->pmOut);

    int devwidth = (fb_painter.device())->width();
    int devheight = (fb_painter.device())->height();

    fb_painter.setPen( Qt::black );
    fb_painter.setBrush( Qt::NoBrush );

    int earthviews = earth_views_per_scanline;

    int pixval[3];
    bool valok[3];
    bool color = bandlist.at(0);

    float flon, flat, fflon, fflat;

    for (int pix = 0 ; pix < earthviews; pix+=8)
    {
        pixval[0] = (int)ptrbaOLCI[0][nbrLine * earthviews + pix];
        valok[0] = pixval[0] < 65535;
        if(color)
        {
            pixval[1] = (int)ptrbaOLCI[1][nbrLine * earthviews + pix];
            pixval[2] = (int)ptrbaOLCI[2][nbrLine * earthviews + pix];
            valok[1] = pixval[1] < 65535;
            valok[2] = pixval[2] < 65535;
        }


        if( valok[0] && (color ? valok[1] && valok[2] : true))
        {
            fflon = (float)(this->longitude[nbrLine * earthviews + pix])/1000000.0;
            fflat = (float)(this->latitude[nbrLine * earthviews + pix])/1000000.0;
            flon = fflon * PI/180.0;
            flat = fflat * PI/180.0;
            sphericalToPixel( flon, flat, posx, posy, devwidth, devheight );
            rgb.setRgb(qRed(row[pix]), qGreen(row[pix]), qBlue(row[pix]));
            fb_painter.setPen(rgb);
            fb_painter.drawPoint( posx , posy );
        }
    }

    fb_painter.end();
    g_mutex.unlock();
}

int SegmentOLCI::UntarSegmentToTemp()
{

    int flags = ARCHIVE_EXTRACT_TIME;
    struct archive *a;
    struct archive *ext;
    struct archive_entry *entry;
    int r;

    QString intarfile = this->fileInfo.absoluteFilePath();

    qDebug() << "Start UntarSegmentToTemp 1 for absolutefilepath " + intarfile;

    QDir direxist(this->fileInfo.completeBaseName());
    if (direxist.exists())
    {
        qDebug() << "Directory " << this->fileInfo.completeBaseName() << " exists !";
        return 0;
    }

    QByteArray array = intarfile.toUtf8();
    const char* p = array.constData();

    a = archive_read_new();
    ext = archive_write_disk_new();
    //archive_read_support_filter_all(a);
    archive_read_support_format_tar(a);

    archive_write_disk_set_options(ext, flags);

    // r = archive_read_open_filename(a, p, 10240); // Note 1
    r = archive_read_open_filename(a, p, 20480);
    if (r != ARCHIVE_OK)
    {
        qDebug() << "Tar file " << intarfile << " not found ....";
        return(1);
    }

//    while (archive_read_next_header(a, &entry) == ARCHIVE_OK)
//    {
//      qDebug() << QString("%1").arg(archive_entry_pathname(entry));
//      archive_read_data_skip(a);  // Note 2
//    }

    int nbrblocks = 1;

    for (;;)
    {
        r = archive_read_next_header(a, &entry);
        if (r == ARCHIVE_EOF)
            break;
        if (r != ARCHIVE_OK)
            qDebug() << "archive_read_next_header() " << QString(archive_error_string(a));
        r = archive_write_header(ext, entry);
        if (r != ARCHIVE_OK)
            qDebug() << "archive_write_header() " << QString(archive_error_string(ext));
        else
        {
            qDebug() << QString("Start copy_data ....%1").arg(nbrblocks);

            copy_data(a, ext);
            r = archive_write_finish_entry(ext);
            if (r != ARCHIVE_OK)
                qDebug() << "archive_write_finish_entry() " << QString(archive_error_string(ext));
            nbrblocks++;
        }
    }

    archive_read_close(a);
    archive_read_free(a);

    archive_write_close(ext);
    archive_write_free(ext);

    return(0);
}


int SegmentOLCI::copy_data(struct archive *ar, struct archive *aw)
{
    int r;
    const void *buff;
    size_t size;
#if ARCHIVE_VERSION_NUMBER >= 3000000
    int64_t offset;
#else
    off_t offset;
#endif


    for (;;) {
        r = archive_read_data_block(ar, &buff, &size, &offset);
        if (r == ARCHIVE_EOF)
            return (ARCHIVE_OK);
        if (r != ARCHIVE_OK)
            return (r);
        r = archive_write_data_block(aw, buff, size, offset);
        if (r != ARCHIVE_OK) {
            qDebug() << "archive_write_data_block() " << QString(archive_error_string(aw));
            return (r);
        }
    }
}

void SegmentOLCI::initializeMemory()
{
    qDebug() << "Initializing OLCI memory";

    bool color = this->bandlist.at(0);

    for(int k = 0; k < (color ? 3 : 1); k++)
    {
        if(ptrbaOLCI[k].isNull())
        {
            ptrbaOLCI[k].reset(new quint16[earth_views_per_scanline * NbrOfLines]);
            ptrbaOLCInormalized[k].reset(new quint16[earth_views_per_scanline * NbrOfLines]);
            qDebug() << QString("Initializing OLCI memory earth views = %1 nbr of lines = %2").arg(earth_views_per_scanline).arg(NbrOfLines);
        }
    }
    qDebug() << "End Initializing OLCI memory";

}



void SegmentOLCI::ComposeSegmentImage(int histogrammethod, bool normalized)
{

    QRgb *row;
    quint16 indexout[3];

    qDebug() << QString("SegmentOLCI::ComposeSegmentImage() segm->startLineNbr = %1").arg(this->startLineNbr);
    qDebug() << QString("SegmentOLCI::ComposeSegmentImage() color = %1 ").arg(bandlist.at(0));
    qDebug() << QString("SegmentOLCI::ComposeSegmentImage() invertthissegment[0] = %1").arg(invertthissegment[0]);
    qDebug() << QString("SegmentOLCI::ComposeSegmentImage() invertthissegment[1] = %1").arg(invertthissegment[1]);
    qDebug() << QString("SegmentOLCI::ComposeSegmentImage() invertthissegment[2] = %1").arg(invertthissegment[2]);

    int color[3];
    quint16 pixval[3];
    quint16 pixval1024[3];

    bool iscolor = bandlist.at(0);
    bool valok[3];

    double gamma = 0.9;
    double gammafactor = 1023 / pow(1023.0, gamma);

    double valgamma = pow( 100, 0.7) * gammafactor;

    for (int line = 0; line < this->NbrOfLines; line++)
    {
        row = (QRgb*)imageptrs->ptrimageOLCI->scanLine(this->startLineNbr + line);
        for (int pixelx = 0; pixelx < earth_views_per_scanline; pixelx++)
        {
            if(normalized) pixval[0] = this->ptrbaOLCInormalized[0][line * earth_views_per_scanline + pixelx];
            else pixval[0] = this->ptrbaOLCI[0][line * earth_views_per_scanline + pixelx];

            if(iscolor)
            {
                if(normalized) pixval[1] = this->ptrbaOLCInormalized[1][line * earth_views_per_scanline + pixelx];
                else pixval[1] = this->ptrbaOLCI[1][line * earth_views_per_scanline + pixelx];
                if(normalized) pixval[2] = this->ptrbaOLCInormalized[2][line * earth_views_per_scanline + pixelx];
                else pixval[2] = this->ptrbaOLCI[2][line * earth_views_per_scanline + pixelx];
            }

            if(opts.usesaturationmask)
            {
                // use of QPolygon saturatedpixels ?
                if(0x001fffff & quality_flags[line * earth_views_per_scanline + pixelx])
                {
                    pixval[0] = imageptrs->stat_max_ch[0];
                    pixval[1] = imageptrs->stat_max_ch[1];
                    pixval[2] = imageptrs->stat_max_ch[2];
                }
//                if(saturatedpixels.contains(QPoint(pixelx, line)))
//                {
//                    pixval[0] = imageptrs->stat_max_ch[0];
//                    pixval[1] = imageptrs->stat_max_ch[1];
//                    pixval[2] = imageptrs->stat_max_ch[2];
//                }
            }

            valok[0] = pixval[0] < 65535;
            valok[1] = pixval[1] < 65535;
            valok[2] = pixval[2] < 65535;

            if( valok[0] && (iscolor ? valok[1] && valok[2] : true))
            {
                for(int k = 0; k < (iscolor ? 3 : 1); k++)
                {
                    if(normalized) pixval1024[k] =  (quint16)qMin(qMax(qRound(1023.0 * (float)(pixval[k] - imageptrs->stat_min_norm_ch[k] ) / (float)(imageptrs->stat_max_norm_ch[k] - imageptrs->stat_min_norm_ch[k])), 0), 1023);
                    else pixval1024[k] =  (quint16)qMin(qMax(qRound(1023.0 * (float)(pixval[k] - imageptrs->stat_min_ch[k] ) / (float)(imageptrs->stat_max_ch[k] - imageptrs->stat_min_ch[k])), 0), 1023);

                    if(histogrammethod == CMB_HISTO_NONE_95) // 95%
                    {
                            if(normalized) indexout[k] =  (quint16)qMin(qMax(qRound(1023.0 * (float)(pixval1024[k] - imageptrs->minRadianceIndexNormalized[k] ) / (float)(imageptrs->maxRadianceIndexNormalized[k] - imageptrs->minRadianceIndexNormalized[k])), 0), 1023);
                            else indexout[k] =  (quint16)qMin(qMax(qRound(1023.0 * (float)(pixval1024[k] - imageptrs->minRadianceIndex[k] ) / (float)(imageptrs->maxRadianceIndex[k] - imageptrs->minRadianceIndex[k])), 0), 1023);
                    }
                    else if(histogrammethod == CMB_HISTO_NONE_100) // 100%
                    {
                            indexout[k] =  pixval1024[k];
                    }

                    if(invertthissegment[k])
                    {
                        if(normalized) color[k] = 255 - imageptrs->lut_norm_ch[k][indexout[k]]/4;
                        else color[k] = 255 - imageptrs->lut_ch[k][indexout[k]]/4;
                    }
                    else
                    {
                        if(histogrammethod == CMB_HISTO_NONE_95 || histogrammethod == CMB_HISTO_NONE_100)
                        {
                            color[k] = (quint16)qMin(qMax(qRound((float)indexout[k]/4), 0), 255);
                        }
                        else if(histogrammethod == CMB_HISTO_EQUALIZE)
                        {
                            if(normalized) color[k] = (quint16)qMin(qMax(qRound((float)imageptrs->lut_norm_ch[k][pixval1024[k]]/4), 0), 255);
                            else color[k] = (quint16)qMin(qMax(qRound((float)imageptrs->lut_ch[k][pixval1024[k]]/4), 0), 255);
                        }
                    }
                }

                row[pixelx] = qRgba(color[0], iscolor ? color[1] : color[0], iscolor ? color[2] : color[0], 255 );

            }
            else
            {
                row[pixelx] = qRgba(0, 0, 0, 0);
            }

        }

        if(opts.imageontextureOnOLCI && line % 2 == 0)
        {
            this->RenderSegmentlineInTextureOLCI( line, row );
            opts.texture_changed = true;
        }

    }
}



void SegmentOLCI::ComposeSegmentGVProjection(int inputchannel, int histogrammethod, bool normalized)
{
    ComposeProjection(GVP, histogrammethod, normalized);
}

void SegmentOLCI::ComposeSegmentLCCProjection(int inputchannel, int histogrammethod, bool normalized)
{
    ComposeProjection(LCC, histogrammethod, normalized);
}

void SegmentOLCI::ComposeSegmentSGProjection(int inputchannel, int histogrammethod, bool normalized)
{
    ComposeProjection(SG, histogrammethod, normalized);
}

void SegmentOLCI::ComposeProjection(eProjections proj, int histogrammethod, bool normalized)
{

    qDebug() << "ComposeProjection(eProjections proj, int histogrammethod, bool normalized) hist = " << histogrammethod << " " << normalized;

    double map_x, map_y;

    float lonpos1, latpos1;

    //g_mutex.lock();

    quint16 pixval[3];

    bool color = bandlist.at(0);
    bool valok[3];

    projectionCoordX.reset(new qint32[NbrOfLines * earth_views_per_scanline]);
    projectionCoordY.reset(new qint32[NbrOfLines * earth_views_per_scanline]);
    projectionCoordValue.reset(new QRgb[NbrOfLines * earth_views_per_scanline]);

    for( int i = 0; i < NbrOfLines; i++)
    {
        for( int j = 0; j < earth_views_per_scanline; j++ )
        {
            projectionCoordX[i * earth_views_per_scanline + j] = 65535;
            projectionCoordY[i * earth_views_per_scanline + j] = 65535;
            projectionCoordValue[i * earth_views_per_scanline + j] = qRgba(0, 0, 0, 0);
        }
    }
    qDebug() << "SegmentOLCIefr::ComposeProjection(eProjections proj)";

    for( int i = 0; i < this->NbrOfLines; i++)
    {
        for( int j = 0; j < this->earth_views_per_scanline ; j++ )
        {
            if(normalized) pixval[0] = ptrbaOLCInormalized[0][i * earth_views_per_scanline + j];
            else pixval[0] = ptrbaOLCI[0][i * earth_views_per_scanline + j];
            valok[0] = pixval[0] >= 0 && pixval[0] < 65535;

            if(color)
            {
                if(normalized) pixval[1] = ptrbaOLCInormalized[1][i * earth_views_per_scanline + j];
                else pixval[1] = ptrbaOLCI[1][i * earth_views_per_scanline + j];

                if(normalized) pixval[2] = ptrbaOLCInormalized[2][i * earth_views_per_scanline + j];
                else pixval[2] = ptrbaOLCI[2][i * earth_views_per_scanline + j];

                valok[1] = pixval[1] > 0 && pixval[1] < 65535;
                valok[2] = pixval[2] > 0 && pixval[2] < 65535;
            }

            if( valok[0] && (color ? valok[1] && valok[2] : true))
            {

                latpos1 = (float)latitude[i * earth_views_per_scanline + j]/1000000.0;
                lonpos1 = (float)longitude[i * earth_views_per_scanline + j]/1000000.0;

//                if((i == 1830 || i == 1831 || i == 1832 || i == 1833) && (j == 2482 || j == 2483 || j == 2484 || j == 2485))
//                    qDebug() << QString("-------------> i = %1 j = %2 latpos1 = %3 lonpos1 = %4").arg(i).arg(j).arg(latpos1).arg(lonpos1);

                if(proj == LCC) // Lambert
                {
                    if(imageptrs->lcc->map_forward_neg_coord(lonpos1 * PI / 180.0, latpos1 * PI / 180.0, map_x, map_y))
                    {
                        MapPixel( i, j, map_x, map_y, color, histogrammethod, normalized);
                    }
                }
                else if(proj == GVP) // General Vertical Perspecitve
                {
                    if(imageptrs->gvp->map_forward_neg_coord(lonpos1 * PI / 180.0, latpos1 * PI / 180.0, map_x, map_y))
                    {
                        MapPixel( i, j, map_x, map_y, color, histogrammethod, normalized);
                    }

                }
                else if(proj == SG) // Stereographic
                {
                    if(imageptrs->sg->map_forward_neg_coord(lonpos1 * PI / 180.0, latpos1 * PI / 180.0, map_x, map_y))
                    {
                        MapPixel( i, j, map_x, map_y, color, histogrammethod, normalized);
                    }
                }
            } else
            {
                projectionCoordX[i * earth_views_per_scanline + j] = 65535;
                projectionCoordY[i * earth_views_per_scanline + j] = 65535;
                projectionCoordValue[i * earth_views_per_scanline + j] = qRgba(0, 0, 0, 0);
            }
        }
    }

}

void SegmentOLCI::MapPixel(int lines, int views, double map_x, double map_y, bool iscolor, int histogrammethod, bool normalized)
{
    int indexout[3];
    quint16 pixval[3];
    quint16 pixval1024[3];

    int color[3];
    QRgb rgbvalue = qRgba(0,0,0,0);

    if(normalized) pixval[0] = ptrbaOLCInormalized[0][lines * earth_views_per_scanline + views];
    else pixval[0] = ptrbaOLCI[0][lines * earth_views_per_scanline + views];

    if(color)
    {
        if(normalized) pixval[1] = ptrbaOLCInormalized[1][lines * earth_views_per_scanline + views];
        else pixval[1] = ptrbaOLCI[1][lines * earth_views_per_scanline + views];

        if(normalized) pixval[2] = ptrbaOLCInormalized[2][lines * earth_views_per_scanline + views];
        else pixval[2] = ptrbaOLCI[2][lines * earth_views_per_scanline + views];
    }

    if (map_x > -15 && map_x < imageptrs->ptrimageProjection->width() + 15 && map_y > -15 && map_y < imageptrs->ptrimageProjection->height() + 15)
    {

        projectionCoordX[lines * earth_views_per_scanline + views] = (qint32)map_x;
        projectionCoordY[lines * earth_views_per_scanline + views] = (qint32)map_y;


        for(int k = 0; k < (iscolor ? 3 : 1); k++)
        {
            if(normalized) pixval1024[k] =  (quint16)qMin(qMax(qRound(1023.0 * (float)(pixval[k] - imageptrs->stat_min_norm_ch[k] ) / (float)(imageptrs->stat_max_norm_ch[k] - imageptrs->stat_min_norm_ch[k])), 0), 1023);
            else pixval1024[k] =  (quint16)qMin(qMax(qRound(1023.0 * (float)(pixval[k] - imageptrs->stat_min_ch[k] ) / (float)(imageptrs->stat_max_ch[k] - imageptrs->stat_min_ch[k])), 0), 1023);

            if(histogrammethod == CMB_HISTO_NONE_95) // 95%
            {
                    if(normalized) indexout[k] =  (quint16)qMin(qMax(qRound(1023.0 * (float)(pixval1024[k] - imageptrs->minRadianceIndexNormalized[k] ) / (float)(imageptrs->maxRadianceIndexNormalized[k] - imageptrs->minRadianceIndexNormalized[k])), 0), 1023);
                    else indexout[k] =  (quint16)qMin(qMax(qRound(1023.0 * (float)(pixval1024[k] - imageptrs->minRadianceIndex[k] ) / (float)(imageptrs->maxRadianceIndex[k] - imageptrs->minRadianceIndex[k])), 0), 1023);
            }
            else if(histogrammethod == CMB_HISTO_NONE_100) // 100%
            {
                    indexout[k] =  pixval1024[k];
            }

            if(invertthissegment[k])
            {
                if(normalized) color[k] = 255 - imageptrs->lut_norm_ch[k][indexout[k]]/4;
                else color[k] = 255 - imageptrs->lut_ch[k][indexout[k]]/4;
            }
            else
            {
                if(histogrammethod == CMB_HISTO_NONE_95 || histogrammethod == CMB_HISTO_NONE_100)
                {
                    color[k] = (quint16)qMin(qMax(qRound((float)indexout[k]/4), 0), 255);
                }
                else if(histogrammethod == CMB_HISTO_EQUALIZE || histogrammethod == CMB_HISTO_EQUALIZE_PROJ)
                {
                    if(normalized) color[k] = (quint16)qMin(qMax(qRound((float)imageptrs->lut_norm_ch[k][pixval1024[k]]/4), 0), 255);
                    else color[k] = (quint16)qMin(qMax(qRound((float)imageptrs->lut_ch[k][pixval1024[k]]/4), 0), 255);
                }
            }
        }


        rgbvalue = qRgba(color[0], iscolor ? color[1] : color[0], iscolor ? color[2] : color[0], 255 );


//        if(opts.sattrackinimage)
//        {
//            if(views == 1598 || views == 1599 || views == 1600 || views == 1601 )
//            {
//                rgbvalue = qRgb(250, 0, 0);
//                if (map_x >= 0 && map_x < imageptrs->ptrimageProjection->width() && map_y >= 0 && map_y < imageptrs->ptrimageProjection->height())
//                    imageptrs->ptrimageProjection->setPixel((int)map_x, (int)map_y, rgbvalue);
//            }
//            else
//            {
//                if (map_x >= 0 && map_x < imageptrs->ptrimageProjection->width() && map_y >= 0 && map_y < imageptrs->ptrimageProjection->height())
//                    imageptrs->ptrimageProjection->setPixel((int)map_x, (int)map_y, rgbvalue);
//                projectionCoordValue[lines * earth_views_per_scanline + views] = rgbvalue;

//            }
//        }
//        else
//        {
            if (map_x >= 0 && map_x < imageptrs->ptrimageProjection->width() && map_y >= 0 && map_y < imageptrs->ptrimageProjection->height())
                imageptrs->ptrimageProjection->setPixel((int)map_x, (int)map_y, rgbvalue);
            projectionCoordValue[lines * earth_views_per_scanline + views] = rgbvalue;
//        }
    }
}

void SegmentOLCI::CalculateDetailCornerPoints()
{


    double statevec = minutes_since_state_vector;
    QSgp4Date sensing = qsensingstart;
    while(statevec <= minutes_since_state_vector + minutes_sensing)
    {
        setupVector(statevec, sensing);
        statevec = statevec + 1.0;
        sensing.AddMin(1.0);
    }

}


void SegmentOLCI::setupVector(double statevec, QSgp4Date sensing)
{

    QEci qeci;

    if(qsgp4.isNull())
        qDebug() << "qsgp4 is NULL !!!";
    qsgp4->getPosition(statevec, qeci);
    QGeodetic qgeo = qeci.ToGeo();

    QVector3D pos;
    QVector3D d3pos = qeci.GetPos_f();
    QVector3D d3vel = qeci.GetVel_f();

    QVector3D d3posnorm = d3pos.normalized();
    QMatrix4x4 mat;
    QVector3D d3scan;

    LonLat2PointRad(qgeo.latitude, qgeo.longitude, &pos, 1.001f);
    QVector3D vec;
    vec.setX(pos.x());
    vec.setY(pos.y());
    vec.setZ(pos.z());
    vec.normalize();

    vecvector.append(vec);

    double e = qtle->Eccenticity();
    double epow2 = e * e;
    double epow3 = e * e * e;

    double span = qeci.GetDate().spanSec(qtle->Epoch());
    double M = fmod(qtle->MeanAnomaly() + (TWOPI * (span/qtle->Period())), TWOPI);
    double C = (2*e - epow3/4)*sin(M) + (5*epow2/4)*sin(2*M) + (13*epow3/12)*sin(3*M);
    double trueAnomaly = M + C;
    double PSO = fmod(qtle->ArgumentPerigee() + trueAnomaly, TWOPI);

    double pitch_steering_angle = - 0.002899 * sin( 2 * PSO);
    double roll_steering_angle = 0.00089 * sin(PSO);
    double yaw_factor = 0.068766 * cos(PSO);
    double yaw_steering_angle = 0.068766 * cos(PSO) * (1 - yaw_factor * yaw_factor/3);

    mat.setToIdentity();
    mat.rotate(yaw_steering_angle * 180/PI, d3pos);  // yaw
    mat.rotate(roll_steering_angle * 180/PI, d3vel); // roll
    mat.rotate(pitch_steering_angle * 180/PI, QVector3D::crossProduct(d3pos,d3vel)); // pitch
    d3scan = mat * QVector3D::crossProduct(d3pos,d3vel);

    //d3scan = QVector3D::crossProduct(d3pos,d3vel);

    QVector3D d3scannorm = d3scan.normalized();

    double delta2 = 23.0 * PI / 180.0;
    double delta1 = 47.0 * PI / 180.0;


    double r = d3pos.length();
    double sindelta = sin(-delta1);
    double cosdelta = cos(-delta1);
    double dd = r * cosdelta - sqrt(XKMPER * XKMPER - r * r * sindelta * sindelta);
    QVector3D d3d = - d3posnorm * cosdelta * dd + d3scannorm * sindelta * dd;
    QVector3D d3earthposfirst = d3pos + d3d;

    QEci qecifirst(d3earthposfirst, d3vel, sensing);
    vectorfirst.append(qecifirst.ToGeo());

    sindelta = sin(delta2);
    cosdelta = cos(delta2);
    dd = r * cosdelta - sqrt(XKMPER * XKMPER - r * r * sindelta * sindelta);
    d3d = - d3posnorm * cosdelta * dd + d3scannorm * sindelta * dd;

    QVector3D d3earthposlast = d3pos + d3d;

    QEci qecilast(d3earthposlast, d3vel, sensing);
    vectorlast.append(qecilast.ToGeo());
}

void SegmentOLCI::recalculateStatsInProjection(bool normalized)
{
    int x, y;

    int statmax[3], statmin[3];
    long active_pixels[3];
    quint16 pixval[3];

    qDebug() << "SegmentOLCI::recalculateStatsInProjection()";

    for(int k = 0; k < 3; k++)
    {
        statmax[k] = 0;
        statmin[k] = 999999;
        active_pixels[k] = 0;
    }

    for(int k = 0; k < (this->bandlist.at(0) ? 3 : 1); k++)
    {
        for (int j = 0; j < this->NbrOfLines; j++)
        {
            for (int i = 0; i < this->earth_views_per_scanline; i++)
            {
                x = *(this->projectionCoordX.data() + j * this->earth_views_per_scanline + i);
                y = *(this->projectionCoordY.data() + j * this->earth_views_per_scanline + i);
                if(x >= 0 && x < imageptrs->ptrimageProjection->width() && y >= 0 && y < imageptrs->ptrimageProjection->height())
                {
                    if(normalized) pixval[k] = this->ptrbaOLCInormalized[k][j * earth_views_per_scanline + i];
                    else pixval[k] = this->ptrbaOLCI[k][j * earth_views_per_scanline + i];

                    if(pixval[k] >= statmax[k])
                        statmax[k] = pixval[k];
                    if(pixval[k] < statmin[k])
                        statmin[k] = pixval[k];
                    active_pixels[k]++;
                }
            }
        }
    }

    for(int k = 0; k < 3; k++)
    {
        stat_max_projection[k] = statmax[k];
        stat_min_projection[k] = statmin[k];
        qDebug() << QString("stat_min_projection[%1] = %2 stat_max_projection[%3] = %4").arg(k).arg(stat_min_projection[k]).arg(k).arg(stat_max_projection[k]);

    }
    active_pixels_projection = active_pixels[0];
    qDebug() << QString("active_pixels_projection = %1").arg(active_pixels_projection);



}

void SegmentOLCI::RecalculateProjection(bool normalized)
{

    quint16 indexout[3];
    quint16 pixval[3];
    int r, g, b;
    QRgb rgbvalue = qRgb(0,0,0);

    int map_x, map_y;

    bool color = bandlist.at(0);
    bool valok[3];

    for( int j = 0; j < this->NbrOfLines; j++)
    {
        for( int i = 0; i < this->earth_views_per_scanline ; i++ )
        {
            for(int k = 0; k < (color ? 3 : 1); k++)
            {
                if(normalized) pixval[k] = this->ptrbaOLCInormalized[k][j * earth_views_per_scanline + i];
                else pixval[k] = this->ptrbaOLCI[k][j * earth_views_per_scanline + i];
            }

            map_x = projectionCoordX[j * this->earth_views_per_scanline + i];
            map_y = projectionCoordY[j * this->earth_views_per_scanline + i];

            if (map_x > -15 && map_x < imageptrs->ptrimageProjection->width() + 15 && map_y > -15 && map_y < imageptrs->ptrimageProjection->height() + 15)
            {


                for(int k = 0; k < (color ? 3 : 1); k++)
                {
                    indexout[k] =  (quint16)qMin(qMax((qRound(1023.0 * (float)( pixval[k] - imageptrs->stat_min_proj_ch[k] ) / (float)(imageptrs->stat_max_proj_ch[k] - imageptrs->stat_min_proj_ch[k]))), 0), 1023);
                }

                if(color)
                {
                    if(invertthissegment[0])
                    {
                        r = 255 - imageptrs->lut_proj_ch[0][indexout[0]]/4;
                    }
                    else
                        r = imageptrs->lut_proj_ch[0][indexout[0]]/4;

                    if(invertthissegment[1])
                    {
                        g = 255 - imageptrs->lut_proj_ch[1][indexout[1]]/4;
                    }
                    else
                        g = imageptrs->lut_proj_ch[1][indexout[1]]/4;

                    if(invertthissegment[2])
                    {
                        b = 255 - imageptrs->lut_proj_ch[2][indexout[2]]/4;
                    }
                    else
                        b = imageptrs->lut_proj_ch[2][indexout[2]]/4;

                    //rgbvalue  = qRgb(imageptrs->lut_ch[0][indexout[0]], imageptrs->lut_ch[1][indexout[1]], imageptrs->lut_ch[2][indexout[2]] );
                    rgbvalue = qRgba(r, g, b, 255);

                }
                else
                {
                    if(invertthissegment[0])
                    {
                        r = 255 - imageptrs->lut_proj_ch[0][indexout[0]]/4;
                    }
                    else
                        r = imageptrs->lut_proj_ch[0][indexout[0]]/4;

                    rgbvalue = qRgba(r, r, r, 255);
                }

                if (map_x >= 0 && map_x < imageptrs->ptrimageProjection->width() && map_y >= 0 && map_y < imageptrs->ptrimageProjection->height())
                    imageptrs->ptrimageProjection->setPixel((int)map_x, (int)map_y, rgbvalue);
                projectionCoordValue[j * earth_views_per_scanline + i] = rgbvalue;


            }
        }
    }

}

SegmentOLCI::~SegmentOLCI()
{

}
