10 PRINT "WEATHER APP"
20 REM FETCH THE LOCATION AND WEATHER DATA
30 OUT 32,1
40 REM PUBLISH THE TEMPERATURE
50 OUT 32,2
60 REM GET THE TEMPERATURE AS A STRING
70 A$=""
80 C=INP(42)
90 IF C = 0 THEN GOTO 120
100 A$=A$+CHR$(C)
110 GOTO 80
120 PRINT "TEMPERATURE "+A$
130 PRINT "END"
