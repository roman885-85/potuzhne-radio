/*  Версія ПОТУЖНОГО РАДІО — своя, не yoRadio.
 *  Номер лежить у firmware/VERSION, час збірки ставить firmware/rebuild.sh
 *  (пише yoBuild.h). У прошивці є ще й відмітка-рядок: сторінка «Оновлення»
 *  знаходить її у вибраному файлі й показує, що саме буде залито.  */
#ifndef yoVersion_h
#define yoVersion_h
const char* prVersion();   /* «1.0» */
const char* prBuild();     /* «11.09.2026 17:40» */
const char* prMarker();    /* «POTUZHNE-RADIO-FW|ES3C28P|1.0|11.09.2026 17:40|» */
#endif
