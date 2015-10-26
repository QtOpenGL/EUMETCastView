#include "segmentviirsdnb.h"
#include "segmentimage.h"

#include "hdf5.h"

#include <QDebug>

extern Options opts;
extern SegmentImage *imageptrs;
#include <QMutex>

extern QMutex g_mutex;

SegmentVIIRSDNB::SegmentVIIRSDNB(QFile *filesegment, SatelliteList *satl, QObject *parent) :
    Segment(parent)
{
    bool ok;

    satlist = satl;

    fileInfo.setFile(*filesegment);
    segment_type = "VIIRSDNB";
    segtype = eSegmentType::SEG_VIIRSDNB;

    //SVDNBC_npp_d20141117_t0837599_e0839241_b15833_c20141117084501709131_eum_ops
    //012345678901234567890
    int sensing_start_year = fileInfo.fileName().mid(12, 4).toInt( &ok , 10);
    int sensing_start_month = fileInfo.fileName().mid(16, 2).toInt( &ok, 10);
    int sensing_start_day = fileInfo.fileName().mid(18, 2).toInt( &ok, 10);
    int sensing_start_hour = fileInfo.fileName().mid(22, 2).toInt( &ok, 10);
    int sensing_start_minute = fileInfo.fileName().mid(24, 2).toInt( &ok, 10);
    int sensing_start_second = fileInfo.fileName().mid(26, 2).toInt( &ok, 10);
    int sensing_start_msecond = fileInfo.fileName().mid(28, 1).toInt( &ok, 10);
    double d_sensing_start_second = (double)sensing_start_second + (double)sensing_start_msecond / 10.0;

    //this->sensing_start_year = sensing_start_year;
    qdatetime_start.setDate(QDate(sensing_start_year, sensing_start_month, sensing_start_day));
    qdatetime_start.setTime(QTime(sensing_start_hour,sensing_start_minute, sensing_start_second,sensing_start_msecond * 100));

    julian_sensing_start = Julian_Date_of_Year(sensing_start_year) +
            DOY( sensing_start_year, sensing_start_month, sensing_start_day ) +
            Fraction_of_Day( sensing_start_hour, sensing_start_minute, d_sensing_start_second )
            + 5.787037e-06; /* Round up to nearest 1 sec */

    julian_sensing_end = julian_sensing_start + Fraction_of_Day( 0, 1, 0);

    qsensingstart = QSgp4Date(sensing_start_year, sensing_start_month, sensing_start_day, sensing_start_hour, sensing_start_minute, d_sensing_start_second);
    qsensingend = qsensingstart;
    qsensingend.AddMin(1.0);

    this->earth_views_per_scanline = 4064;
    this->NbrOfLines = 768;

    Satellite nss_2;
    ok = satlist->GetSatellite(37849, &nss_2);
    line1 = nss_2.line1;
    line2 = nss_2.line2;

    //line1 = "1 33591U 09005A   11039.40718334  .00000086  00000-0  72163-4 0  8568";
    //line2 = "2 33591  98.8157 341.8086 0013952 344.4168  15.6572 14.11126791103228";
    double epoch = line1.mid(18,14).toDouble(&ok);
    julian_state_vector = Julian_Date_of_Epoch(epoch);

    qtle = new QTle(nss_2.sat_name, line1, line2, QTle::wgs72);
    qsgp4 = new QSgp4( *qtle );


    minutes_since_state_vector = ( julian_sensing_start - julian_state_vector ) * MIN_PER_DAY; //  + (1.0/12.0) / 60.0;
    minutes_sensing = 86.0/60.0;

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

    //qDebug() << QString("---> lon = %1 lat = %2  hours_since_state_vector = %3").arg(lon_start_deg).arg(lat_start_deg).arg( hours_since_state_vector);

    geolatitude = NULL;
    geolongitude = NULL;

    ptrbaVIIRSDNB = NULL;

    projectionCoordX = NULL;
    projectionCoordY = NULL;
    projectionCoordValue = NULL;

    tiepoints_lat = NULL;
    tiepoints_lon = NULL;
    aligncoef = NULL;
    expanscoef = NULL;

    latMax = 0.0;
    lonMax = 0.0;
    latMin = 999.0;
    lonMin = 999.0;
    CalculateCornerPoints();
    invertthissegment[0] = false;
    invertthissegment[1] = false;
    invertthissegment[2] = false;

}

SegmentVIIRSDNB::~SegmentVIIRSDNB()
{
    cleanupMemory();
}

void SegmentVIIRSDNB::initializeMemory()
{
    qDebug() << "Initializing VIIRSDNB memory";
    if(ptrbaVIIRSDNB == NULL)
    {
        ptrbaVIIRSDNB = new float[earth_views_per_scanline * NbrOfLines];
        qDebug() << QString("Initializing VIIRSDNB memory earth views = %1 nbr of lines = %2").arg(earth_views_per_scanline).arg(NbrOfLines);
        bImageMemory = true;
    }
}

void SegmentVIIRSDNB::resetMemory()
{
    if( geolatitude != NULL)
    {
        delete [] geolatitude;
        geolatitude = NULL;
    }
    if( geolongitude != NULL)
    {
        delete [] geolongitude;
        geolongitude = NULL;
    }

    if( projectionCoordX != NULL)
    {
        delete [] projectionCoordX;
        projectionCoordX = NULL;
    }
    if( projectionCoordY != NULL)
    {
        delete [] projectionCoordY;
        projectionCoordY = NULL;
    }
    if( projectionCoordValue != NULL)
    {
        delete [] projectionCoordValue;
        projectionCoordValue = NULL;
    }
    if(ptrbaVIIRSDNB != NULL)
    {
        delete ptrbaVIIRSDNB;
        ptrbaVIIRSDNB = NULL;
    }

    bImageMemory = false;

}

void SegmentVIIRSDNB::cleanupMemory()
{
    resetMemory();
}

Segment *SegmentVIIRSDNB::ReadSegmentInMemory()
{

    FILE*   f = NULL;
    BZFILE* b;
    int     nBuf;
    char    buf[ 32768 ];
    int     bzerror;
    hid_t   h5_file_id, radiance_id, latitude_id, longitude_id;
    hid_t   aligncoef_id, expanscoef_id;
    herr_t  h5_status;

    bool tempfileexist;

    QString basename = this->fileInfo.baseName() + ".h5";
    QFile tfile(basename);
    tempfileexist = tfile.exists();

    qDebug() << QString("file %1  tempfileexist = %2").arg(basename).arg(tempfileexist);

    QFile fileout(basename);
    fileout.open(QIODevice::WriteOnly);
    QDataStream streamout(&fileout);


    if((b = BZ2_bzopen(this->fileInfo.absoluteFilePath().toLatin1(),"rb"))==NULL)
    {
        qDebug() << "error in BZ2_bzopen";
    }

    bzerror = BZ_OK;
    while ( bzerror == BZ_OK )
    {
        nBuf = BZ2_bzRead ( &bzerror, b, buf, 32768 );
        if ( bzerror == BZ_OK || bzerror == BZ_STREAM_END)
        {
            streamout.writeRawData(buf, nBuf);
        }
    }

    BZ2_bzclose ( b );

    fileout.close();

    tiepoints_lat = new float[96 * 316];
    tiepoints_lon = new float[96 * 316];
    aligncoef = new float[252];
    expanscoef = new float[252];

    geolongitude = new float[NbrOfLines * earth_views_per_scanline];
    geolatitude = new float[NbrOfLines * earth_views_per_scanline];


    if( (h5_file_id = H5Fopen(basename.toLatin1(), H5F_ACC_RDONLY, H5P_DEFAULT)) < 0)
        qDebug() << "File " << basename << " not open !!";

    if((radiance_id = H5Dopen2(h5_file_id, "/All_Data/VIIRS-DNB-SDR_All/Radiance", H5P_DEFAULT)) < 0)
        qDebug() << "Dataset " << "/All_Data/VIIRS-DNB-SDR_All/Radiance" << " is not open !!";
    else
        qDebug() << "Dataset " << "/All_Data/VIIRS-DNB-SDR_All/Radiance" << " is open !!  ok ok ok ";

    if((h5_status = H5Dread (radiance_id, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL,
                             H5P_DEFAULT, ptrbaVIIRSDNB)) < 0)
        qDebug() << "Unable to read radiance dataset";



    latitude_id = H5Dopen2(h5_file_id, "/All_Data/VIIRS-DNB-GEO_All/Latitude", H5P_DEFAULT);
    longitude_id = H5Dopen2(h5_file_id, "/All_Data/VIIRS-DNB-GEO_All/Longitude", H5P_DEFAULT);
    aligncoef_id = H5Dopen2(h5_file_id, "/All_Data/VIIRS-DNB-GEO_All/AlignmentCoefficient", H5P_DEFAULT);
    expanscoef_id = H5Dopen2(h5_file_id, "/All_Data/VIIRS-DNB-GEO_All/ExpansionCoefficient", H5P_DEFAULT);


    if((h5_status = H5Dread (latitude_id, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL,
                             H5P_DEFAULT, tiepoints_lat)) < 0)
        fprintf(stderr, "unable to read latitude dataset");

    if((h5_status = H5Dread (longitude_id, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL,
                             H5P_DEFAULT, tiepoints_lon)) < 0)
        fprintf(stderr, "unable to read longitude dataset");

    if((h5_status = H5Dread (aligncoef_id, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL,
                             H5P_DEFAULT, aligncoef)) < 0)
        fprintf(stderr, "unable to read AlignmentCoefficient dataset");

    if((h5_status = H5Dread (expanscoef_id, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL,
                             H5P_DEFAULT, expanscoef)) < 0)
        fprintf(stderr, "unable to read ExpansionCoefficient dataset");

    int i, j;

    for( j = 0; j < 16; j++)
        s[j] = (float)((j + 0.5)/16.0);

    /*
    for (j = 0; j < 4; j++) {
        for (i = 0; i < 6; i++)
           cout << " " <<  ptrbaVIIRSDNB[j * earth_views_per_scanline + i];
        cout << endl;
    }
*/
    /*    cout  << "tie point latitude : " << endl;
    for (j = 0; j < 2; j++) {
        for (i = 100; i < 102; i++)
           cout << " " <<  tiepoints_lat[j * 201 + i];
        cout << endl;
    }

*/
    //    cout  << "Calc geo lat and lon" << endl;

    //    for(int itrack = 0; itrack < 48; itrack++)
    //    {
    //        for(int iscan = 0; iscan < 200; iscan++)
    //        {
    //            CalcGeoLocations(itrack, iscan);
    //        }
    //    }

    //this->LonLatMax();

    /*    cout << "alpha voor iscan = 100 :" << endl;
    for (j = 0; j < 16; j++) {
        for (i = 0; i < 16; i++)
           cout << " " <<  alpha[j][i];
        cout << endl;
    }


    cout << "geolatitude  :" << endl;
    for (j = 0; j < 16; j++) {
        for (i = 1600; i < 1616; i++)
           cout << " " <<  geolatitude[j * earth_views_per_scanline + i];
        cout << endl;
    }

    cout << "geolatitude  :" << endl;
    for (j = 0; j < NbrOfLines; j+=767) {
        for (i = 0; i < earth_views_per_scanline; i+=3199)
           cout << " " <<  geolatitude[j * earth_views_per_scanline + i];
        cout << endl;
    }
    cout << "geolongitude  :" << endl;
    for (j = 0; j < NbrOfLines; j+=767) {
        for (i = 0; i < earth_views_per_scanline; i+=3199)
           cout << " " <<  geolongitude[j * earth_views_per_scanline + i];
        cout << endl;
    }

*/

/*
    stat_max = -1.0E31;
    stat_min = 1.0E31;

    long count0 = 0;
    long counttot = 0;

    for (j = 0; j < NbrOfLines; j++) {
        for (i = 0; i < earth_views_per_scanline; i++)
        {
            counttot++;
            if(ptrbaVIIRSDNB[j * earth_views_per_scanline + i] <= 0.0)
                count0++;
            //if(ptrbaVIIRSDNB[j * earth_views_per_scanline + i] >= 9.5E-12 && ptrbaVIIRSDNB[j * earth_views_per_scanline + i] < 9.0E-08)
            {
                if(ptrbaVIIRSDNB[j * earth_views_per_scanline + i] >= stat_max)
                {
                    stat_max = ptrbaVIIRSDNB[j * earth_views_per_scanline + i];
                }
                if(ptrbaVIIRSDNB[j * earth_views_per_scanline + i] < stat_min)
                    stat_min = ptrbaVIIRSDNB[j * earth_views_per_scanline + i];
            }
        }
    }

    qDebug() << QString("ptrbaVIIRSDNB min_ch = %1 max_ch = %2").arg(stat_min).arg(stat_max);
    qDebug() << QString("count0 = %1 counttot = %2").arg(count0).arg(counttot);
*/

    delete [] tiepoints_lat;
    delete [] tiepoints_lon;
    delete [] aligncoef;
    delete [] expanscoef;

    tiepoints_lat = NULL;
    tiepoints_lon = NULL;
    aligncoef = NULL;
    expanscoef = NULL;

    h5_status = H5Dclose (radiance_id);
    h5_status = H5Dclose (latitude_id);
    h5_status = H5Dclose (longitude_id);
    h5_status = H5Dclose (aligncoef_id);
    h5_status = H5Dclose (expanscoef_id);

    h5_status = H5Fclose (h5_file_id);

    return this;
}

Segment *SegmentVIIRSDNB::ReadDatasetsInMemory()
{
    qDebug() << "Segment *SegmentVIIRS::ReadDatasetsInMemory()";

    hid_t   h5_file_id, radiance_id;
    herr_t  h5_status;

    bool tempfileexist;

    QString basename = this->fileInfo.baseName() + ".h5";

    {
        QFile tfile(basename);
        tempfileexist = tfile.exists();
    }

    qDebug() << QString("trying H5Fopen basename = %1 exist = %2").arg(basename).arg(tempfileexist);

    if( (h5_file_id = H5Fopen(basename.toLatin1(), H5F_ACC_RDONLY, H5P_DEFAULT)) < 0)
        qDebug() << "File " << basename << " not open !!";
    else
        qDebug() << "File " << basename << " is open !! ------------";

    if((radiance_id = H5Dopen2(h5_file_id, "/All_Data/VIIRS-DNB-SDR_All/Radiance", H5P_DEFAULT)) < 0)
        qDebug() << "Dataset " << "/All_Data/VIIRS-DNB-SDR_All/Radiance" << " is not open !!";
    else
        qDebug() << "Dataset " << "/All_Data/VIIRS-DNB-SDR_All/Radiance" << " is open !!  ok ok ok ";

    if((h5_status = H5Dread (radiance_id, H5T_NATIVE_USHORT, H5S_ALL, H5S_ALL,
                             H5P_DEFAULT, ptrbaVIIRSDNB)) < 0)
        qDebug() << "Unable to read radiance dataset";


    for(int k = 0; k < 3; k++)
    {
        stat_max_ch[k] = 0;
        stat_min_ch[k] = 9999999;
    }


    for (int j = 0; j < NbrOfLines; j++) {
        for (int i = 0; i < earth_views_per_scanline; i++)
        {
            if(ptrbaVIIRSDNB[j * earth_views_per_scanline + i] >= stat_max_ch[0])
                stat_max_ch[0] = ptrbaVIIRSDNB[j * earth_views_per_scanline + i];
            if(ptrbaVIIRSDNB[j * earth_views_per_scanline + i] < stat_min_ch[0])
                stat_min_ch[0] = ptrbaVIIRSDNB[j * earth_views_per_scanline + i];
        }
    }

    h5_status = H5Dclose (radiance_id);

    h5_status = H5Fclose (h5_file_id);

    return this;

}




void SegmentVIIRSDNB::GetAlpha( float &ascan, float &atrack, int rels, int relt, int iscan)
{
    ascan = s[rels] + s[rels] * (1 - s[rels]) * expanscoef[iscan] + s[relt] * (1 - s[relt]) * aligncoef[iscan];
    atrack = s[relt];
}

void SegmentVIIRSDNB::CalcGeoLocations(int itrack, int iscan)  // 0 <= itrack < 48 ; 0 <= iscan < 200
{
    int iA, iB, iC, iD;
    int jA, jB, jC, jD;
    float lat_A, lat_B, lat_C, lat_D;
    float lon_A, lon_B, lon_C, lon_D;
    quint16 val_A, val_B, val_C, val_D;

    iA = 2 * itrack;
    jA = iscan;
    iB = 2 * itrack;
    jB = iscan + 1;
    iC = 2 * itrack + 1;
    jC = iscan + 1;
    iD = 2 * itrack + 1;
    jD = iscan;

    lat_A = tiepoints_lat[iA * 201 + jA];
    lat_B = tiepoints_lat[iB * 201 + jB];
    lat_C = tiepoints_lat[iC * 201 + jC];
    lat_D = tiepoints_lat[iD * 201 + jD];

//    if(itrack == 0)
//        qDebug() << QString("itrack = %1 iscan = %2 Lat tiepoint A = %3 B = %4 C = %5 D = %6").arg(itrack).arg(iscan).arg(lat_A).arg(lat_B).arg(lat_C).arg(lat_D);

    lon_A = tiepoints_lon[iA * 201 + jA];
    lon_B = tiepoints_lon[iB * 201 + jB];
    lon_C = tiepoints_lon[iC * 201 + jC];
    lon_D = tiepoints_lon[iD * 201 + jD];


    val_A = ptrbaVIIRSDNB[((itrack * 16)) * earth_views_per_scanline + (iscan * 16)];
    val_B = ptrbaVIIRSDNB[((itrack * 16)) * earth_views_per_scanline + (iscan * 16) + 15];
    val_C = ptrbaVIIRSDNB[((itrack * 16) + 15) * earth_views_per_scanline + (iscan * 16) + 15];
    val_D = ptrbaVIIRSDNB[((itrack * 16) + 15) * earth_views_per_scanline + (iscan * 16)];

    if( val_A == 0 || val_A > 65528 || val_B == 0 || val_B > 65528 || val_C == 0 || val_C > 65528 || val_D == 0 || val_D > 65528)
    {
        quint16 minval = Min(val_A, val_B, val_C, val_D);

        for(int relt = 0; relt < 16; relt++)
        {
            for(int rels = 0; rels < 16; rels++)
            {
                if(ptrbaVIIRSDNB[((itrack * 16) + relt) * earth_views_per_scanline + (iscan * 16) + rels] == 0 || ptrbaVIIRSDNB[((itrack * 16) + relt) * earth_views_per_scanline + (iscan * 16) + rels] >= 65528)
                {
                    geolatitude[((itrack * 16) + relt) * earth_views_per_scanline + (iscan * 16) + rels] = 65535;
                    geolongitude[((itrack * 16) + relt) * earth_views_per_scanline + (iscan * 16) + rels] = 65535;
                }
            }
        }
    }

//    if(itrack == 0)
//        qDebug() << QString("itrack = %1 iscan = %2 Lon tiepoint A = %3 B = %4 C = %5 D = %6").arg(itrack).arg(iscan).arg(lon_A).arg(lon_B).arg(lon_C).arg(lon_D);

    float themin = Minf(lon_A, lon_B, lon_C, lon_D);
    float themax = Maxf(lon_A, lon_B, lon_C, lon_D);


    if (Maxf(abs(lat_A), abs(lat_B), abs(lat_C), abs(lat_D)) > 60.0 || (themax - themin) > 90.0)
        interpolateViaVector(itrack, iscan, lon_A, lon_B, lon_C, lon_D, lat_A, lat_B, lat_C, lat_D);
    else
        interpolateViaLonLat(itrack, iscan, lon_A, lon_B, lon_C, lon_D, lat_A, lat_B, lat_C, lat_D);

}

void SegmentVIIRSDNB::interpolateViaLonLat(int itrack, int iscan, float lon_A, float lon_B, float lon_C, float lon_D, float lat_A, float lat_B, float lat_C, float lat_D)
{

    float ascan, atrack;
    float lat_1, lat_2, lat;
    float lon_1, lon_2, lon;

    for(int relt = 0; relt < 16; relt++)
    {
        for(int rels = 0; rels < 16; rels++)
        {
            GetAlpha(ascan, atrack, rels, relt, iscan);
            // 96 x 201

            lat_1 = (1 - ascan) * lat_A + ascan * lat_B;
            lat_2 = (1 - ascan) * lat_D + ascan * lat_C;
            lat = (1 - atrack) * lat_1 + atrack * lat_2;

            lon_1 = (1 - ascan) * lon_A + ascan * lon_B;
            lon_2 = (1 - ascan) * lon_D + ascan * lon_C;
            lon = (1 - atrack) * lon_1 + atrack * lon_2;

            geolatitude[((itrack * 16) + relt) * earth_views_per_scanline + (iscan * 16) + rels] = lat;
            geolongitude[((itrack * 16) + relt) * earth_views_per_scanline + (iscan * 16) + rels] = lon;


        }
    }

//    if( itrack == 0 && iscan == 0)
//    {
//        for (int relt = 0; relt < 16; relt++) {
//            for (int rels = 0; rels < 16; rels++)
//               cout << " " <<  geolatitude[((itrack * 16) + relt) * earth_views_per_scanline + (iscan * 16) + rels];
//            cout << endl;
//        }
//    }

}

void SegmentVIIRSDNB::interpolateViaVector(int itrack, int iscan, float lon_A, float lon_B, float lon_C, float lon_D, float lat_A, float lat_B, float lat_C, float lat_D)
{
    float ascan, atrack;
    float lon, lat;

    // Earth Centred vectors
    float lat_A_rad = lat_A * PI / 180.0;
    float lon_A_rad = lon_A * PI / 180.0;
    float lat_B_rad = lat_B * PI / 180.0;
    float lon_B_rad = lon_B * PI / 180.0;
    float lat_C_rad = lat_C * PI / 180.0;
    float lon_C_rad = lon_C * PI / 180.0;
    float lat_D_rad = lat_D * PI / 180.0;
    float lon_D_rad = lon_D * PI / 180.0;

    float x_A_ec = cos(lat_A_rad) * cos(lon_A_rad);
    float y_A_ec = cos(lat_A_rad) * sin(lon_A_rad);
    float z_A_ec = sin(lat_A_rad);

    float x_B_ec = cos(lat_B_rad) * cos(lon_B_rad);
    float y_B_ec = cos(lat_B_rad) * sin(lon_B_rad);
    float z_B_ec = sin(lat_B_rad);

    float x_C_ec = cos(lat_C_rad) * cos(lon_C_rad);
    float y_C_ec = cos(lat_C_rad) * sin(lon_C_rad);
    float z_C_ec = sin(lat_C_rad);

    float x_D_ec = cos(lat_D_rad) * cos(lon_D_rad);
    float y_D_ec = cos(lat_D_rad) * sin(lon_D_rad);
    float z_D_ec = sin(lat_D_rad);


    float x1, y1, z1;
    float x2, y2, z2;
    float x, y, z;
    float lon_deg, lat_deg;

    for(int relt = 0; relt < 16; relt++)
    {
        for(int rels = 0; rels < 16; rels++)
        {
            GetAlpha(ascan, atrack, rels, relt, iscan);
            // 96 x 201

            x1 = (1 - ascan) * x_A_ec + ascan * x_B_ec;
            y1 = (1 - ascan) * y_A_ec + ascan * y_B_ec;
            z1 = (1 - ascan) * z_A_ec + ascan * z_B_ec;

            x2 = (1 - ascan) * x_D_ec + ascan * x_C_ec;
            y2 = (1 - ascan) * y_D_ec + ascan * y_C_ec;
            z2 = (1 - ascan) * z_D_ec + ascan * z_C_ec;

            x = (1 - atrack) * x1 + atrack * x2;
            y = (1 - atrack) * y1 + atrack * y2;
            z = (1 - atrack) * z1 + atrack * z2;

            lon_deg = atan2(y, x) * 180.0/PI;
            lat_deg = atan2(z, sqrt(x * x + y * y)) * 180.0/PI;

            geolatitude[((itrack * 16) + relt) * earth_views_per_scanline + (iscan * 16) + rels] = lat_deg;
            geolongitude[((itrack * 16) + relt) * earth_views_per_scanline + (iscan * 16) + rels] = lon_deg;


        }
    }

//    if( itrack == 0)
//    {
//        cout << "geolatitude" << endl;
//        for (int relt = 0; relt < 16; relt++) {
//            for (int rels = 0; rels < 16; rels++)
//               cout << " " <<  geolatitude[((itrack * 16) + relt) * earth_views_per_scanline + (iscan * 16) + rels];
//            cout << endl;
//        }
//        cout << "geolongitude" << endl;
//        for (int relt = 0; relt < 16; relt++) {
//            for (int rels = 0; rels < 16; rels++)
//               cout << " " <<  geolongitude[((itrack * 16) + relt) * earth_views_per_scanline + (iscan * 16) + rels];
//            cout << endl;
//        }
//    }

}

int SegmentVIIRSDNB::ReadNbrOfLines()
{
    return NbrOfLines;
}

//void SegmentVIIRSDNB::ComposeSegmentImage()
//{

//    QRgb *row;
//    int indexout;

//    qDebug() << QString("SegmentVIIRSDNB::ComposeSegmentImage() segm->startLineNbr = %1").arg(this->startLineNbr);
//    qDebug() << QString("SegmentVIIRSDNB::ComposeSegmentImage() invertthissegment[0] = %1").arg(invertthissegment[0]);

//    float pixval;
//    int r;

//    for (int line = 0; line < this->NbrOfLines; line++)
//    {
//        row = (QRgb*)imageptrs->ptrimageViirs->scanLine(this->startLineNbr + line);
//        for (int pixelx = 0; pixelx < earth_views_per_scanline; pixelx++)
//        {
//            pixval = *(this->ptrbaVIIRSDNB + line * earth_views_per_scanline + pixelx);
//            indexout =  (int)(255 * ( pixval - lowerlimit ) / (upperlimit - lowerlimit));
//            indexout = indexout > 255 ? 255 : indexout;
//            indexout = indexout < 0 ? 0 : indexout;
//            r = indexout;
//            row[pixelx] = qRgb(r, r, r );
//        }
//    }
//}

void SegmentVIIRSDNB::ComposeSegmentImageWindow(float lowerlimit, float upperlimit)
{

    QRgb *row;
    int indexout;

    float pixval;
    int r;

    for (int line = 0; line < this->NbrOfLines; line++)
    {
        row = (QRgb*)imageptrs->ptrimageViirs->scanLine(this->startLineNbr + line);
        for (int pixelx = 0; pixelx < earth_views_per_scanline; pixelx++)
        {
            pixval = *(this->ptrbaVIIRSDNB + line * earth_views_per_scanline + pixelx);
            indexout =  (int)(255 * ( pixval - lowerlimit ) / (upperlimit - lowerlimit));
            indexout = indexout > 255 ? 255 : indexout;
            indexout = indexout < 0 ? 0 : indexout;
            r = indexout;
            row[pixelx] = qRgb(r, r, r );
        }
    }
}


void SegmentVIIRSDNB::ComposeSegmentLCCProjection(int inputchannel)
{
    ComposeProjection(LCC);
}

void SegmentVIIRSDNB::ComposeSegmentGVProjection(int inputchannel)
{
    ComposeProjection(GVP);
}

void SegmentVIIRSDNB::ComposeSegmentSGProjection(int inputchannel)
{
    ComposeProjection(SG);
}

void SegmentVIIRSDNB::ComposeProjection(eProjections proj)
{

    double map_x, map_y;

    float lonpos1, latpos1;

    //g_mutex.lock();

    int pixval[3];

    bool color = bandlist.at(0);
    bool valok[3];

    projectionCoordX = new qint32[NbrOfLines * earth_views_per_scanline];
    projectionCoordY = new qint32[NbrOfLines * earth_views_per_scanline];
    projectionCoordValue = new QRgb[NbrOfLines * earth_views_per_scanline];

    for( int i = 0; i < NbrOfLines; i++)
    {
        for( int j = 0; j < earth_views_per_scanline ; j++ )
        {
            projectionCoordX[i * earth_views_per_scanline + j] = 65535;
            projectionCoordY[i * earth_views_per_scanline + j] = 65535;
            projectionCoordValue[i * earth_views_per_scanline + j] = qRgba(0, 0, 0, 0);
        }
    }
    qDebug() << "SegmentVIIRS::ComposeProjection(eProjections proj)";

    for( int i = 0; i < this->NbrOfLines; i++)
    {
        for( int j = 0; j < this->earth_views_per_scanline ; j++ )
        {
            pixval[0] = ptrbaVIIRSDNB[i * earth_views_per_scanline + j];
            valok[0] = pixval[0] > 0 && pixval[0] < 65528;

            if(color)
            {
                pixval[1] = ptrbaVIIRS[1][i * earth_views_per_scanline + j];
                pixval[2] = ptrbaVIIRS[2][i * earth_views_per_scanline + j];
                valok[1] = pixval[1] > 0 && pixval[1] < 65528;
                valok[2] = pixval[2] > 0 && pixval[2] < 65528;
            }


            if( valok[0] && (color ? valok[1] && valok[2] : true))
            {
                latpos1 = geolatitude[i * earth_views_per_scanline + j];
                lonpos1 = geolongitude[i * earth_views_per_scanline + j];

                if(proj == LCC) //Lambert
                {
                    if(imageptrs->lcc->map_forward_neg_coord(lonpos1 * PI / 180.0, latpos1 * PI / 180.0, map_x, map_y))
                    {
                        MapPixel( i, j, map_x, map_y, color);
                    }
                }
                else if(proj == GVP) // General Vertical Perspecitve
                {
                    if(imageptrs->gvp->map_forward_neg_coord(lonpos1 * PI / 180.0, latpos1 * PI / 180.0, map_x, map_y))
                    {
                        MapPixel( i, j, map_x, map_y, color);
                    }

                }
                else if(proj == SG) // Stereographic
                {
                    if(imageptrs->sg->map_forward_neg_coord(lonpos1 * PI / 180.0, latpos1 * PI / 180.0, map_x, map_y))
                    {
                        MapPixel( i, j, map_x, map_y, color);
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

/*
    int maxX = 0;
    int maxY = 0;

    fprintf(stderr, "projectionCoordX \n");
    for( int i = 0; i < 16; i++) //this->NbrOfLines - 1; i++)
    {
        for( int j = 0; j < 16; j++) //this->earth_views_per_scanline - 1 ; j++ )
        {
            if(projectionCoordX[i * earth_views_per_scanline + j] < 10000 && projectionCoordX[i * earth_views_per_scanline + j + 1] < 10000)
            {
                if(maxX < abs(projectionCoordX[i * earth_views_per_scanline + j] - projectionCoordX[i * earth_views_per_scanline + j + 1]) )
                    maxX = abs(projectionCoordX[i * earth_views_per_scanline + j] - projectionCoordX[i * earth_views_per_scanline + j + 1]);
                if(maxY < abs(projectionCoordY[i * earth_views_per_scanline + j] - projectionCoordY[i * earth_views_per_scanline + j + 1]) )
                    maxY = abs(projectionCoordY[i * earth_views_per_scanline + j] - projectionCoordY[i * earth_views_per_scanline + j + 1]);
            }

            fprintf(stderr, "%u ", projectionCoordX[i * earth_views_per_scanline + j]);
        }

        fprintf(stderr, "\n");
    }

    fprintf(stderr, "projectionCoordY \n");

    for( int i = 0; i < 16; i++) //this->NbrOfLines - 1; i++)
    {
        for( int j = 0; j < 16; j++) //this->earth_views_per_scanline - 1 ; j++ )
        {
            fprintf(stderr, "%u ", projectionCoordY[i * earth_views_per_scanline + j]);
        }

        fprintf(stderr, "\n");
    }

    qDebug() << QString("===============>>>>>>>>>>> maxX = %1 maxY = %2").arg(maxX).arg(maxY);

*/


   // g_mutex.unlock();

}

//void SegmentVIIRS::ComposeProjectionConcurrent()
//{
//    if( geolatitude == 0x0)
//    {
//        qDebug() << "pointer to geolatitude = 0x0 !!!!!!!!!!!";
//    }

//    bool color = bandlist.at(0);
//    bool valok[3];
//    int pixval[3];

//    LonLatMax();

//    int col, row;
//    double lon_rad, lat_rad;
//    int counter = 0;
//    qDebug() << "=====> start SegmentVIIRS::ComposeProjectionConcurrent";
//    int cntTotal = imageptrs->ptrimageProjection->height() * imageptrs->ptrimageProjection->width();

//    //testlookupLonLat(55.888, 73.900, col, row);
//    //return;

//    g_mutex.lock();

//    for (int j = 0; j < imageptrs->ptrimageProjection->height(); j++)
//    {
//        for (int i = 0; i < imageptrs->ptrimageProjection->width(); i++)
//        {
//            if (imageptrs->gvp->map_inverse(i, j, lon_rad, lat_rad))
//            {
//                if(this->lookupLonLat(lon_rad*180.0/PI, lat_rad*180/PI, col, row))
//                {
//                    if(col == 1500 && row == 400)
//                        counter++;
//                    pixval[0] = ptrbaVIIRSDNB[col * earth_views_per_scanline + row];
//                    valok[0] = pixval[0] > 0 && pixval[0] < 65528;
//                    if(color)
//                    {
//                        pixval[1] = ptrbaVIIRS[1][col * earth_views_per_scanline + row];
//                        pixval[2] = ptrbaVIIRS[2][col * earth_views_per_scanline + row];
//                        valok[1] = pixval[1] > 0 && pixval[1] < 65528;
//                        valok[2] = pixval[2] > 0 && pixval[2] < 65528;
//                    }

//                    if( valok[0] && (color ? valok[1] && valok[2] : true))
//                        MapPixel(col, row, i, j, color);
//                }

//            }
//        }
//    }

//    g_mutex.unlock();

//    qDebug() << "=====> end SegmentVIIRS::ComposeProjectionConcurrent counter = " << counter << " from total = " << cntTotal;

//}

void SegmentVIIRSDNB::LonLatMax()
{

    lonMin = +180.0;
    lonMax = -180.0;
    latMin = +90.0;
    latMax = -90.0;

    for (int j = 0; j < NbrOfLines; j+=1)
    {
        for (int i = 0; i < earth_views_per_scanline; i+=1)
        {
           if(geolatitude[j * earth_views_per_scanline + i] > latMax)
               latMax = geolatitude[j * earth_views_per_scanline + i];
           if(geolatitude[j * earth_views_per_scanline + i] <= latMin)
               latMin = geolatitude[j * earth_views_per_scanline + i];
           if(geolongitude[j * earth_views_per_scanline + i] > lonMax)
               lonMax = geolongitude[j * earth_views_per_scanline + i];
           if(geolongitude[j * earth_views_per_scanline + i] <= lonMin)
               lonMin = geolongitude[j * earth_views_per_scanline + i];
        }
     }

    qDebug() << QString("Minimum Latitude = %1 ; Maximum Latitude = %2").arg(latMin).arg(latMax);
    qDebug() << QString("Minimum Longitude = %1 ; Maximum Longitude = %2").arg(lonMin).arg(lonMax);


}


void SegmentVIIRSDNB::RenderSegmentlineInTextureVIIRS( int nbrLine, QRgb *row )
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


    for (int pix = 0 ; pix < earthviews; pix+=2)
    {
        pixval[0] = ptrbaVIIRSDNB[nbrLine * earth_views_per_scanline + pix];
        valok[0] = pixval[0] < 65528 && pixval[0] > 0;
        if(color)
        {
            pixval[1] = ptrbaVIIRS[1][nbrLine * earth_views_per_scanline + pix];
            pixval[2] = ptrbaVIIRS[2][nbrLine * earth_views_per_scanline + pix];
            valok[1] = pixval[1] < 65528 && pixval[1] > 0;
            valok[2] = pixval[2] < 65528 && pixval[2] > 0;
        }


        if( valok[0] && (color ? valok[1] && valok[2] : true))
        {
            sphericalToPixel( this->geolongitude[nbrLine * earth_views_per_scanline + pix] * PI/180.0, this->geolatitude[nbrLine * earth_views_per_scanline + pix] * PI/180.0, posx, posy, devwidth, devheight );
            rgb.setRgb(qRed(row[pix]), qGreen(row[pix]), qBlue(row[pix]));
            fb_painter.setPen(rgb);
            fb_painter.drawPoint( posx , posy );
        }
    }

    fb_painter.end();
    g_mutex.unlock();
}


void SegmentVIIRSDNB::MapPixel( int lines, int views, double map_x, double map_y, bool color)
{
    int indexout;
    int pixval;
    int r, g, b;
    QRgb rgbvalue = qRgb(0,0,0);

    pixval = ptrbaVIIRSDNB[lines * earth_views_per_scanline + views];


//    if (map_x >= 0 && map_x < imageptrs->ptrimageProjection->width() && map_y >= 0 && map_y < imageptrs->ptrimageProjection->height())
    if (map_x > -5 && map_x < imageptrs->ptrimageProjection->width() + 5 && map_y > -5 && map_y < imageptrs->ptrimageProjection->height() + 5)
    {

        projectionCoordX[lines * earth_views_per_scanline + views] = (qint32)map_x;
        projectionCoordY[lines * earth_views_per_scanline + views] = (qint32)map_y;

        indexout =  (int)(255 * ( pixval - imageptrs->stat_min_ch[0] ) / (imageptrs->stat_max_ch[0] - imageptrs->stat_min_ch[0]));
        indexout = ( indexout > 255 ? 255 : indexout );


        if(color)
        {
            if(invertthissegment[0])
            {
                r = 255 - imageptrs->lut_ch[0][indexout];
            }
            else
                r = imageptrs->lut_ch[0][indexout];
            if(invertthissegment[1])
            {
                g = 255 - imageptrs->lut_ch[1][indexout];
            }
            else
                g = imageptrs->lut_ch[1][indexout];
            if(invertthissegment[2])
            {
                b = 255 - imageptrs->lut_ch[2][indexout];
            }
            else
                b = imageptrs->lut_ch[2][indexout];

            //rgbvalue  = qRgb(imageptrs->lut_ch[0][indexout[0]], imageptrs->lut_ch[1][indexout[1]], imageptrs->lut_ch[2][indexout[2]] );
            rgbvalue = qRgba(r, g, b, 255);

        }
        else
        {
            if(invertthissegment[0])
            {
                r = 255 - imageptrs->lut_ch[0][indexout];
            }
            else
                r = imageptrs->lut_ch[0][indexout];

             rgbvalue = qRgba(r, r, r, 255);
        }

        if(opts.sattrackinimage)
        {
            if(views == 1598 || views == 1599 || views == 1600 || views == 1601 )
            {
                rgbvalue = qRgb(250, 0, 0);
                if (map_x >= 0 && map_x < imageptrs->ptrimageProjection->width() && map_y >= 0 && map_y < imageptrs->ptrimageProjection->height())
                    imageptrs->ptrimageProjection->setPixel((int)map_x, (int)map_y, rgbvalue);
            }
            else
            {
                if (map_x >= 0 && map_x < imageptrs->ptrimageProjection->width() && map_y >= 0 && map_y < imageptrs->ptrimageProjection->height())
                    imageptrs->ptrimageProjection->setPixel((int)map_x, (int)map_y, rgbvalue);
                projectionCoordValue[lines * earth_views_per_scanline + views] = rgbvalue;

            }
        }
        else
        {
            if (map_x >= 0 && map_x < imageptrs->ptrimageProjection->width() && map_y >= 0 && map_y < imageptrs->ptrimageProjection->height())
                imageptrs->ptrimageProjection->setPixel((int)map_x, (int)map_y, rgbvalue);
            projectionCoordValue[lines * earth_views_per_scanline + views] = rgbvalue;
        }

    }
}

float SegmentVIIRSDNB::Minf(const float v11, const float v12, const float v21, const float v22)
{
    float Minimum = v11;

    if( Minimum > v12 )
            Minimum = v12;
    if( Minimum > v21 )
            Minimum = v21;
    if( Minimum > v22 )
            Minimum = v22;

    return Minimum;
}

float SegmentVIIRSDNB::Maxf(const float v11, const float v12, const float v21, const float v22)
{
    int Maximum = v11;

    if( Maximum < v12 )
            Maximum = v12;
    if( Maximum < v21 )
            Maximum = v21;
    if( Maximum < v22 )
            Maximum = v22;

    return Maximum;
}

qint32 SegmentVIIRSDNB::Min(const qint32 v11, const qint32 v12, const qint32 v21, const qint32 v22)
{
    qint32 Minimum = v11;

    if( Minimum > v12 )
            Minimum = v12;
    if( Minimum > v21 )
            Minimum = v21;
    if( Minimum > v22 )
            Minimum = v22;

    return Minimum;
}

qint32 SegmentVIIRSDNB::Max(const qint32 v11, const qint32 v12, const qint32 v21, const qint32 v22)
{
    int Maximum = v11;

    if( Maximum < v12 )
            Maximum = v12;
    if( Maximum < v21 )
            Maximum = v21;
    if( Maximum < v22 )
            Maximum = v22;

    return Maximum;
}

//    float m00_A = -sin(lon_A);
//    float m01_A = cos(lon_A);
//    float m02_A = 0;
//    float m10_A = -sin(lat_A)*cos(lon_A);
//    float m11_A = -sin(lat_A)*sin(lon_A);
//    float m12_A = cos(lat_A);
//    float m20_A = cos(lat_A) * cos(lon_A);
//    float m21_A = cos(lat_A) * sin(lon_A);
//    float m22_A = sin(lat_A);

//    float m00_B = -sin(lon_B);
//    float m01_B = cos(lon_B);
//    float m02_B = 0;
//    float m10_B = -sin(lat_B)*cos(lon_B);
//    float m11_B = -sin(lat_B)*sin(lon_B);
//    float m12_B = cos(lat_B);
//    float m20_B = cos(lat_B) * cos(lon_B);
//    float m21_B = cos(lat_B) * sin(lon_B);
//    float m22_B = sin(lat_B);

//    float m00_C = -sin(lon_C);
//    float m01_C = cos(lon_C);
//    float m02_C = 0;
//    float m10_C = -sin(lat_C)*cos(lon_C);
//    float m11_C = -sin(lat_C)*sin(lon_C);
//    float m12_C = cos(lat_C);
//    float m20_C = cos(lat_C) * cos(lon_C);
//    float m21_C = cos(lat_C) * sin(lon_C);
//    float m22_C = sin(lat_C);

//    float m00_D = -sin(lon_D);
//    float m01_D = cos(lon_D);
//    float m02_D = 0;
//    float m10_D = -sin(lat_D)*cos(lon_D);
//    float m11_D = -sin(lat_D)*sin(lon_D);
//    float m12_D = cos(lat_D);
//    float m20_D = cos(lat_D) * cos(lon_D);
//    float m21_D = cos(lat_D) * sin(lon_D);
//    float m22_D = sin(lat_D);

//// Pixel centred
//    float x_A_pc = m00_A * x_A_ec + m10_A * y_A_ec + m20_A * z_A_ec;
//    float y_A_pc = m01_A * x_A_ec + m11_A * y_A_ec + m21_A * z_A_ec;
//    float z_A_pc = m02_A * x_A_ec + m12_A * y_A_ec + m22_A * z_A_ec;

//    float x_B_pc = m00_B * x_B_ec + m10_B * y_B_ec + m20_B * z_B_ec;
//    float y_B_pc = m01_B * x_B_ec + m11_B * y_B_ec + m21_B * z_B_ec;
//    float z_B_pc = m02_B * x_B_ec + m12_B * y_B_ec + m22_B * z_B_ec;

//    float x_C_pc = m00_C * x_C_ec + m10_C * y_C_ec + m20_C * z_C_ec;
//    float y_C_pc = m01_C * x_C_ec + m11_C * y_C_ec + m21_C * z_C_ec;
//    float z_C_pc = m02_C * x_C_ec + m12_C * y_C_ec + m22_C * z_C_ec;

//    float x_D_pc = m00_D * x_D_ec + m10_D * y_D_ec + m20_D * z_D_ec;
//    float y_D_pc = m01_D * x_D_ec + m11_D * y_D_ec + m21_D * z_D_ec;
//    float z_D_pc = m02_D * x_D_ec + m12_D * y_D_ec + m22_D * z_D_ec;
