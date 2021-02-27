bool isAscii(String utf8string) {
    /*! Check, if an arbitrary UTF-8 string only consists of ASCII characters

    @return true, if ASCII compliant
    */
   for (unsigned int i=0; i<utf8string.length(); i++) {
       unsigned char c=utf8string[i];
       if ((c & 0x80) != 0) return false;
   }
   return true;
}

String utf8ToLatin(String utf8string, char invalid_char='_') {
    /*! Convert an arbitrary UTF-8 string into latin1 (ISO 8859-1)

    Note: This converts arbitrary multibyte utf-8 strings to latin1 on best-effort
    basis. Characters that are not in the target code-page are replaced by invalid_char.
    The conversion is aborted, if an invalid UTF8-encoding is encountered.
    @param utf8string utf8-encoded string
    @param invalid_char character that is used for to replace characters that are not in latin1
    @return latin1 (ISO 8859-1) encoded string
    */
    String latin="";
    unsigned char c,nc;
    for (unsigned int i=0; i<utf8string.length(); i++) {
        c=utf8string[i];
        if ((c & 0x80)==0) { // 1 byte utf8
            latin+=(char)c;
            continue;
        } else {
            if ((c & 0b11100000)==0b11000000) { // 2 byte utf8
                if (i<utf8string.length()-1) {
                    nc=utf8string[i+1];
                    switch (c) {
                        case 0xc2:
                            latin+=(char)nc;
                        break;
                        case 0xc3:
                            nc += 0x40;
                            latin+=(char)nc;
                        break;
                        default:
                            latin+=(char)invalid_char;
                        break;
                    }
                    i+=1;
                    continue;
                } else { // damaged utf8
                    latin+=(char)invalid_char;
                    return latin;
                }
            } else {
                if ((c & 0b11110000) == 0b11100000) { // 3 byte utf8
                    i+=2;
                    latin+=(char)invalid_char;
                    continue;
                } else {
                    if ((c & 0b11111000) == 0b11110000) { // 4 byte utf8
                        i+=3;
                        latin+=(char)invalid_char;
                        continue;
                    } else { // damaged utf8
                        latin+=(char)invalid_char;
                        return latin;  // we can't continue parsing
                    }
                }
            }
        }
    }
    return latin;
}

String latinToUtf8(String latin) {
    /*! Convert a latin1 (ISO 8859-1) string into UTF-8

    @param latin ISO 8869-1 (latin1) encoded string
    @return UTF8-encoded string  
    */

    String utf8str="";
    unsigned char c;
    for (unsigned int i=0; i<latin.length(); i++) {
        c=latin[i];
        if (c<0x80) utf8str+=(char)c;
        else {
            if (c<0xc0) {
                utf8str+=(char)0xc2;
                utf8str+=(char)c;
            } else {
                utf8str+=(char)0xc3;
                c-=0x40;
                utf8str+=(char)c;
            }
        }
    }
    return utf8str;
}
