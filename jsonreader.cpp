

#ifdef NDEBUG
#define wxDEBUG_LEVEL 0
#endif

#include "jsonreader.h"

#include <wx/mstream.h>
#include <wx/sstream.h>
#include <wx/debug.h>
#include <wx/log.h>



/*! \class wxJSONReader
 \brief The JSON parser

 The class is a JSON parser which reads a JSON formatted text and stores
 values in the \c wxJSONValue structure.
 The ctor accepts two parameters: the \e style flag, which controls how
 much error-tolerant should the parser be and an integer which is
 the maximum number of errors and warnings that have to be reported
 (the default is 30).

 If the JSON text document does not contain an open/close JSON character the
 function returns an \b invalid value object; in other words, the
 wxJSONValue::IsValid() function returns FALSE.
 This is the case of a document that is empty or contains only
 whitespaces or comments.
 If the document contains a starting object/array character immediatly
 followed by a closing object/array character
 (i.e.: \c {} ) then the function returns an \b empty array or object
 JSON value.
 This is a valid JSON object of type wxJSONTYPE_OBJECT or wxJSONTYPE_ARRAY
 whose wxJSONValue::Size() function returns ZERO.

 \par JSON text

 The wxJSON parser just skips all characters read from the
 input JSON text until the start-object '{' or start-array '[' characters
 are encontered (see the GetStart() function).
 This means that the JSON input text may contain anything
 before the first start-object/array character except these two chars themselves
 unless they are included in a C/C++ comment.
 Comment lines that apear before the first start array/object character,
 are non ignored if the parser is constructed with the wxJSONREADER_STORE_COMMENT
 flag: they are added to the comment's array of the root JSON value.

 Note that the parsing process stops when the internal DoRead() function
 returns. Because that function is recursive, the top-level close-object
 '}' or close-array ']' character cause the top-level DoRead() function
 to return thus stopping the parsing process regardless the EOF condition.
 This means that the JSON input text may contain anything \b after
 the top-level close-object/array character.
 Here are some examples:

 Returns a wxJSONTYPE_INVALID value (invalid JSON value)
 \code
 \endcode

 Returns a wxJSONTYPE_OBJECT value of Size() = 0
 \code
   {
   }
 \endcode

 Returns a wxJSONTYPE_ARRAY value of Size() = 0
 \code
   [
   ]
 \endcode

 Text before and after the top-level open/close characters is ignored.
 \code
   This non-JSON text does not cause the parser to report errors or warnings
   {
   }
   This non-JSON text does not cause the parser to report errors or warnings
 \endcode


 \par Extensions

 The wxJSON parser recognizes all JSON text plus some extensions
 that are not part of the JSON syntax but that many other JSON
 implementations do recognize.
 If the input text contains the following non-JSON text, the parser
 reports the situation as \e warnings and not as \e errors unless
 the parser object was constructed with the wxJSONREADER_STRICT
 flag. In the latter case the wxJSON parser is not tolerant.

 \li C/C++ comments: the parser recognizes C and C++ comments.
    Comments can optionally be stored in the value they refer
    to and can also be written back to the JSON text document.
    To know more about comment storage see \ref wxjson_comments

 \li case tolerance: JSON syntax states that the literals \c null,
    \c true and \c false must be lowercase; the wxJSON parser
    also recognizes mixed case literals such as, for example,
    \b Null or \b FaLSe.  A \e warning is emitted.

 \li wrong or missing closing character: wxJSON parser is tolerant
    about the object / array closing character. When an open-array
    character '[' is encontered, the parser expects the
    corresponding close-array character ']'. If the character
    encontered is a close-object char '}' a warning is reported.
    A warning is also reported if the character is missing when
    the end-of-file is reached.

 \li multi-line strings: this feature allows a JSON string type to be
    splitted in two or more lines as in the standard C/C++
    languages. The drawback is that this feature is error-prone
    and you have to use it with care.
    For more info about this topic read \ref wxjson_tutorial_style_split

 Note that you can control how much error-tolerant should the parser be
 and also you can specify how many and what extensions are recognized.
 See the constructor's parameters for more details.

 \par Unicode vs ANSI

 The parser can read JSON text from two very different kind of objects:

 \li a string object (\b wxString)
 \li a stream object (\b wxInputStream)

 When the input is from a string object, the character represented in the
 string is platform- and mode- dependant; in other words, characters are
 represented differently: in ANSI builds they depend on the charset in use
 and in Unicode builds they depend on the platform (UCS-2 on win32, UCS-4
 or UTF-8 on GNU/Linux).

 When the input is from a stream object, the only recognized encoding format
 is UTF-8 for both ANSI and Unicode builds.

 \par Example:

 \code
  wxJSONValue  value;
  wxJSONReader reader;

  wxFFileInputStream jsonText( _T("filename.utf8"), _T("r"));

  int numErrors = reader.Parse( jsonText, &value );

  if ( numErrors > 0 )  {
    ::MessageBox( _T("Error reading the input file"));
  }
 \endcode

 Starting from version 1.1.0 the wxJSON reader and the writer has changed in
 their internal organization.
 To know more about ANSI and Unicode mode read \ref wxjson_tutorial_unicode.
*/



#if wxDEBUG_LEVEL > 0
static const wxChar* traceMask = _T("traceReader");
static const wxChar* storeTraceMask = _T("StoreComment");
#endif

/*!
 Construct a JSON parser object with the given parameters.

 JSON parser objects should always be constructed on the stack but
 it does not hurt to have a global JSON parser.

 \param flags this paramter controls how much error-tolerant should the
        parser be

 \param maxErrors the maximum number of errors (and warnings, too) that are
    reported by the parser. When the number of errors reaches this limit,
    the parser stops to read the JSON input text and no other error is
    reported.

 The \c flag parameter is the combination of ZERO or more of the
 following constants OR'ed toghether:

 \li wxJSONREADER_ALLOW_COMMENTS: C/C++ comments are recognized by the
     parser; a warning is reported by the parser
 \li wxJSONREADER_STORE_COMMENTS: C/C++ comments, if recognized, are
     stored in the value they refer to and can be rewritten back to
     the JSON text
 \li wxJSONREADER_CASE: the parser recognizes mixed-case literal strings
 \li wxJSONREADER_MISSING: the parser allows missing or wrong close-object
     and close-array characters
 \li wxJSONREADER_MULTISTRING: strings may be splitted in two or more
     lines
 \li wxJSONREADER_COMMENTS_AFTER: if STORE_COMMENTS if defined, the parser
     assumes that comment lines apear \b before the value they
     refer to unless this constant is specified. In the latter case,
     comments apear \b after the value they refer to.
 \li wxJSONREADER_NOUTF8_STREAM: suppress UTF-8 conversion when reading a
         string value from a stream: the reader assumes that the input stream
         is encoded in ANSI format and not in UTF-8; only meaningfull in ANSI
         builds, this flag is simply ignored in Unicode builds.

 You can also use the following shortcuts to specify some predefined
 flag's combinations:

  \li wxJSONREADER_STRICT: all wxJSON extensions are reported as errors, this
      is the same as specifying a ZERO value as \c flags.
  \li wxJSONREADER_TOLERANT: this is the same as ALLOW_COMMENTS | CASE |
      MISSING | MULTISTRING; all wxJSON extensions are turned on but comments
      are not stored in the value objects.

 \par Example:

 The following code fragment construct a JSON parser, turns on all
 wxJSON extensions and also stores C/C++ comments in the value object
 they refer to. The parser assumes that the comments apear before the
 value:

 \code
   wxJSONReader reader( wxJSONREADER_TOLERANT | wxJSONREADER_STORE_COMMENTS );
   wxJSONValue  root;
   int numErrors = reader.Parse( jsonText, &root );
 \endcode
*/
wxJSONReader::wxJSONReader( int flags, int maxErrors )
{
    m_flags     = flags;
    m_maxErrors = maxErrors;
    m_peekChar  = -1;
    m_noUtf8    = false;
#if !defined( wxJSON_USE_UNICODE )
    if ( m_flags & wxJSONREADER_NOUTF8_STREAM )    {
        m_noUtf8 = true;
    }
#endif

}

wxJSONReader::~wxJSONReader()
{
}

/*!
 The two overloaded versions of the \c Parse() function read a
 JSON text stored in a wxString object or in a wxInputStream
 object.

 If \c val is a NULL pointer, the function does not store the
 values: it can be used as a JSON checker in order to check the
 syntax of the document.
 Returns the number of \b errors found in the document.
 If the returned value is ZERO and the parser was constructed
 with the \c wxJSONREADER_STRICT flag, then the parsed document
 is \e well-formed and it only contains valid JSON text.

 If the \c wxJSONREADER_TOLERANT flag was used in the parser's
 constructor, then a return value of ZERO
 does not mean that the document is \e well-formed because it may
 contain comments and other extensions that are not fatal for the
 wxJSON parser but other parsers may fail to recognize.
 You can use the \c GetWarningCount() function to know how many
 wxJSON extensions are present in the JSON input text.

 Note that the JSON value object \c val is not cleared by this
 function unless its type is of the wrong type.
 In other words, if \c val is of type wxJSONTYPE_ARRAY and it already
 contains 10 elements and the input document starts with a
 '[' (open-array char) then the elements read from the document are
 \b appended to the existing ones.

 On the other hand, if the text document starts with a '{' (open-object) char
 then this function must change the type of the \c val object to
 \c wxJSONTYPE_OBJECT and the old content of 10 array elements will be lost.

 \par Different input types

 The real parsing process in done using UTF-8 streams. If the input is
 from a \b wxString object, the Parse function first converts the input string
 in a temporary \b wxMemoryInputStream which contains the UTF-8 conversion
 of the string itself.
 Next, the overloaded Parse function is called.

 @param doc    the JSON text that has to be parsed
 @param val    the wxJSONValue object that contains the parsed text; if NULL the
         parser do not store anything but errors and warnings are reported
 @return the total number of errors encontered
*/
int
wxJSONReader:: Parse( const wxString& doc, wxJSONValue* val )
{
#if !defined( wxJSON_USE_UNICODE )
    bool noUtf8_bak = m_noUtf8;
    m_noUtf8 = true;
#endif

    char* readBuff = 0;
    wxCharBuffer utf8CB = doc.ToUTF8();
#if !defined( wxJSON_USE_UNICODE )
    wxCharBuffer ansiCB( doc.c_str());
    if ( m_noUtf8 )    {
        readBuff = ansiCB.data();
    }
    else    {
        readBuff = utf8CB.data();
    }
#else
        readBuff = utf8CB.data();
#endif

    wxCharBuffer stream = readBuff;
    wxMemoryInputStream is( stream.data(), stream.length() );

    int numErr = Parse( is, val );
#if !defined( wxJSON_USE_UNICODE )
    m_noUtf8 = noUtf8_bak;
#endif
    return numErr;
}

int
wxJSONReader::Parse( wxInputStream& is, wxJSONValue* val )
{
    wxJSONValue temp;
    m_level    = 0;
    m_depth    = 0;
    m_lineNo   = 1;
    m_colNo    = 1;
    m_peekChar = -1;
    m_errors.clear();
    m_warnings.clear();

    if ( val == 0 )  {
        val = &temp;
    }
    wxASSERT( val );

    m_next       = val;
    m_next->SetLineNo( -1 );
    m_lastStored = 0;
    m_current    = 0;

    int ch = GetStart( is );
    switch ( ch )  {
        case '{' :
        val->SetType( wxJSONTYPE_OBJECT );
        break;
    case '[' :
        val->SetType( wxJSONTYPE_ARRAY );
        break;
    default :
        AddError( _T("Cannot find a start object/array character" ));
        return m_errors.size();
        break;
    }

    ch = DoRead( is, *val );
    return m_errors.size();
}


/*!
 This is the first function called by the Parse() function and it searches
 the input stream for the starting character of a JSON text and returns it.
 JSON text start with '{' or '['.
 If the two starting characters are inside a C/C++ comment, they
 are ignored.
 Returns the JSON-text start character or -1 on EOF.

 @param is    the input stream that contains the JSON text
 @return -1 on errors or EOF; one of '{' or '['
*/
int
wxJSONReader::GetStart( wxInputStream& is )
{
    int ch = 0;
    do  {
        switch ( ch )  {
            case 0 :
                ch = ReadChar( is );
                break;
            case '{' :
                return ch;
                break;
            case '[' :
                return ch;
                break;
            case '/' :
                ch = SkipComment( is );
                StoreComment( 0 );
                break;
            default :
                ch = ReadChar( is );
                break;
        }
    } while ( ch >= 0 );
    return ch;
}

const wxArrayString&
wxJSONReader::GetErrors() const
{
    return m_errors;
}

const wxArrayString&
wxJSONReader::GetWarnings() const
{
    return m_warnings;
}

/*!
 The function returns the number of times the recursive \c DoRead function was
 called in the parsing process thus returning the maximum depth of the JSON
 input text.
*/
int
wxJSONReader::GetDepth() const
{
    return m_depth;
}



int
wxJSONReader::GetErrorCount() const
{
    return m_errors.size();
}

int
wxJSONReader::GetWarningCount() const
{
    return m_warnings.size();
}


/*!
 The function returns the next byte from the UTF-8 stream as an INT.
 In case of errors or EOF, the function returns -1.
 The function also updates the \c m_lineNo and \c m_colNo data
 members and converts all CR+LF sequence in LF.

 This function only returns one byte UTF-8 (one code unit)
 at a time and not Unicode code points.
 The only reason for this function is to process line and column
 numbers.

 @param is    the input stream that contains the JSON text
 @return the next char (one single byte) in the input stream or -1 on error or EOF
*/
int
wxJSONReader::ReadChar( wxInputStream& is )
{
    if ( is.Eof())    {
        return -1;
    }

    unsigned char ch = is.GetC();
    size_t last = is.LastRead();
    if ( last == 0 )    {
        return -1;
    }


    if ( ch == '\r' )  {
        m_colNo = 1;
        int nextChar = PeekChar( is );
        if ( nextChar == -1 )  {
            return -1;
        }
        else if ( nextChar == '\n' )    {
            ch = is.GetC();
        }
    }
    if ( ch == '\n' )  {
        ++m_lineNo;
        m_colNo = 1;
    }
    else  {
        ++m_colNo;
    }
    return (int) ch;
}


/*!
 This function just calls the \b Peek() function on the stream
 and returns it.

 @param is    the input stream that contains the JSON text
 @return the next char (one single byte) in the input stream or -1 on error or EOF
*/
int
wxJSONReader::PeekChar( wxInputStream& is )
{
    if ( !is.Eof() )
        return is.Peek();
    else
        return -1;
}


/*!
 This is a recursive function that is called by \c Parse()
 and by the \c DoRead() function itself when a new object /
 array character is encontered.
 The function returns when a EOF condition is encontered or
 when the corresponding close-object / close-array char is encontered.
 The function also increments the \c m_level
 data member when it is entered and decrements it on return.
 It also sets \c m_depth equal to \c m_level if \c m_depth is
 less than \c m_level.

 The function is the heart of the wxJSON parser class but it is
 also very easy to understand because JSON syntax is very
 easy.

 Returns the last close-object/array character read or -1 on EOF.

 @param is    the input stream that contains the JSON text
 @param parent the JSON value object that is the parent of all subobjects
         read by the function until the next close-object/array (for
         the top-level \c DoRead function \c parent is the root JSON object)
 @return one of close-array or close-object char or -1 on error or EOF
*/
int
wxJSONReader::DoRead( wxInputStream& is, wxJSONValue& parent )
{
    ++m_level;
    if ( m_depth < m_level )    {
        m_depth = m_level;
    }

    wxJSONValue value( wxJSONTYPE_INVALID );

    m_next = &value;
    m_current = &parent;
    m_current->SetLineNo( m_lineNo );
    m_lastStored = 0;

    wxString  key;

    int ch=0;

    do {
        switch ( ch )  {
            case 0 :
                ch = ReadChar( is );
                break;
            case ' ' :
            case '\t' :
            case '\n' :
            case '\r' :
                ch = SkipWhiteSpace( is );
                break;
            case -1 :
                break;
            case '/' :
                ch = SkipComment( is );
                StoreComment( &parent );
                break;

            case '{' :
                if ( parent.IsObject() ) {
                    if ( key.empty() )   {
                        AddError( _T("\'{\' is not allowed here (\'name\' is missing") );
                    }
                    if ( value.IsValid() )   {
                        AddError( _T("\'{\' cannot follow a \'value\'") );
                          }
                }
                else if ( parent.IsArray() )  {
                    if ( value.IsValid() )   {
                        AddError( _T("\'{\' cannot follow a \'value\' in JSON array") );
                    }
                }
                else  {
                    wxJSON_ASSERT( 0 );
                }

                value.SetType( wxJSONTYPE_OBJECT );
                ch = DoRead( is, value );
                break;

            case '}' :
                if ( !parent.IsObject() )  {
                    AddWarning( wxJSONREADER_MISSING,
                    _T("Trying to close an array using the \'}\' (close-object) char" ));
                }
                StoreValue( ch, key, value, parent );
                m_current = &parent;
                m_next    = 0;
                m_current->SetLineNo( m_lineNo );
                ch = ReadChar( is );
                return ch;
                break;

            case '[' :
                if ( parent.IsObject() ) {
                    if ( key.empty() )   {
                        AddError( _T("\'[\' is not allowed here (\'name\' is missing") );
                    }
                    if ( value.IsValid() )   {
                        AddError( _T("\'[\' cannot follow a \'value\' text") );
                    }
                }
                else if ( parent.IsArray())  {
                    if ( value.IsValid() )   {
                        AddError( _T("\'[\' cannot follow a \'value\'") );
                    }
                }
                else  {
                    wxJSON_ASSERT( 0 );
                }
                value.SetType( wxJSONTYPE_ARRAY );
                ch = DoRead( is, value );
                break;

            case ']' :
                if ( !parent.IsArray() )  {
                    AddWarning( wxJSONREADER_MISSING,
                    _T("Trying to close an object using the \']\' (close-array) char" ));
                }
                StoreValue( ch, key, value, parent );
                m_current = &parent;
                m_next    = 0;
                m_current->SetLineNo( m_lineNo );
                return 0;
                break;

            case ',' :
                StoreValue( ch, key, value, parent );
                key.clear();
                ch = ReadChar( is );
                break;

            case '\"' :
                ch = ReadString( is, value );
                m_current = &value;
                m_next    = 0;
                break;

            case '\'' :
                ch = ReadMemoryBuff( is, value );
                m_current = &value;
                m_next    = 0;
                break;

            case ':' :
                m_current = &value;
                m_current->SetLineNo( m_lineNo );
                m_next    = 0;
                if ( !parent.IsObject() )  {
                    AddError( _T( "\':\' can only used in object's values" ));
                }
                else if ( !value.IsString() )  {
                    AddError( _T( "\':\' follows a value which is not of type \'string\'" ));
                }
                else if ( !key.empty() )  {
                    AddError( _T( "\':\' not allowed where a \'name\' string was already available" ));
                }
                else  {
                    key = value.AsString();
                    value.SetType( wxJSONTYPE_INVALID );
                }
                ch = ReadChar( is );
                break;

            default :
                m_current = &value;
                m_current->SetLineNo( m_lineNo );
                m_next    = 0;
                ch = ReadValue( is, ch, value );
                break;
        }
    } while ( ch >= 0 );

    if ( parent.IsArray() )  {
        AddWarning( wxJSONREADER_MISSING, _T("\']\' missing at end of file"));
    }
    else if ( parent.IsObject() )  {
        AddWarning( wxJSONREADER_MISSING, _T("\'}\' missing at end of file"));
    }
    else  {
        wxJSON_ASSERT( 0 );
    }

    StoreValue( ch, key, value, parent );

    --m_level;
    return ch;
}

/*!
 The function is called by \c DoRead() when a the comma
 or a close-object/array character is encontered and stores the current
 value read by the parser in the parent object.
 The function checks that \c value is not invalid and that \c key is
 not an empty string if \c parent is an object.

 \param ch    the character read: a comma or close objecty/array char
 \param key    the \b key string: must be empty if \c parent is an array
 \param value    the current JSON value to be stored in \c parent
 \param parent    the JSON value that is the parent of \c value.
 \return none
*/
void
wxJSONReader::StoreValue( int ch, const wxString& key, wxJSONValue& value, wxJSONValue& parent )
{
    wxLogTrace( traceMask, _T("(%s) ch=%d char=%c"), __PRETTY_FUNCTION__, ch, (char) ch);
    wxLogTrace( traceMask, _T("(%s) value=%s"), __PRETTY_FUNCTION__, value.AsString().c_str());

    m_current = 0;
    m_next    = &value;
    m_lastStored = 0;
    m_next->SetLineNo( -1 );

    if ( !value.IsValid() && key.empty() ) {
        if ( ch == '}' || ch == ']' )  {
            m_lastStored = 0;
            wxLogTrace( traceMask, _T("(%s) key and value are empty, returning"),
                             __PRETTY_FUNCTION__);
        }
        else  {
            AddError( _T("key or value is missing for JSON value"));
        }
    }
    else  {
        if ( parent.IsObject() )  {
            if ( !value.IsValid() ) {
                AddError( _T("cannot store the value: \'value\' is missing for JSON object type"));
             }
             else if ( key.empty() ) {
                AddError( _T("cannot store the value: \'key\' is missing for JSON object type"));
            }
            else  {
                wxLogTrace( traceMask, _T("(%s) adding value to key:%s"),
                     __PRETTY_FUNCTION__, key.c_str());
                parent[key] = value;
                m_lastStored = &(parent[key]);
                m_lastStored->SetLineNo( m_lineNo );
            }
        }
        else if ( parent.IsArray() ) {
            if ( !value.IsValid() ) {
                    AddError( _T("cannot store the item: \'value\' is missing for JSON array type"));
            }
            if ( !key.empty() ) {
                AddError( _T("cannot store the item: \'key\' (\'%s\') is not permitted in JSON array type"), key);
            }
            wxLogTrace( traceMask, _T("(%s) appending value to parent array"),
                                 __PRETTY_FUNCTION__ );
            parent.Append( value );
            const wxJSONInternalArray* arr = parent.AsArray();
            wxJSON_ASSERT( arr );
            m_lastStored = &(arr->Last());
            m_lastStored->SetLineNo( m_lineNo );
        }
        else  {
            wxJSON_ASSERT( 0 );
        }
    }
    value.SetType( wxJSONTYPE_INVALID );
    value.ClearComments();
}

/*!
 The overloaded versions of this function add an error message to the
 error's array stored in \c m_errors.
 The error message is formatted as follows:

 \code
   Error: line xxx, col xxx - <error_description>
 \endcode

 The \c msg parameter is the description of the error; line's and column's
 number are automatically added by the functions.
 The \c fmt parameter is a format string that has the same syntax as the \b printf
 function.
 Note that it is the user's responsability to provide a format string suitable
 with the arguments: another string or a character.
*/
void
wxJSONReader::AddError( const wxString& msg )
{
    wxString err;
    err.Printf( _T("Error: line %d, col %d - %s"), m_lineNo, m_colNo, msg.c_str() );

    wxLogTrace( traceMask, _T("(%s) %s"), __PRETTY_FUNCTION__, err.c_str());

    if ( (int) m_errors.size() < m_maxErrors )  {
        m_errors.Add( err );
    }
    else if ( (int) m_errors.size() == m_maxErrors )  {
        m_errors.Add( _T("ERROR: too many error messages - ignoring further errors"));
    }
}

void
wxJSONReader::AddError( const wxString& fmt, const wxString& str )
{
    wxString s;
    s.Printf( fmt.c_str(), str.c_str() );
    AddError( s );
}

void
wxJSONReader::AddError( const wxString& fmt, wxChar c )
{
    wxString s;
    s.Printf( fmt.c_str(), c );
    AddError( s );
}

/*!
 The warning description is as follows:
 \code
   Warning: line xxx, col xxx - <warning_description>
 \endcode

 Warning messages are generated by the parser when the JSON
 text that has been read is not well-formed but the
 error is not fatal and the parser recognizes the text
 as an extension to the JSON standard (see the parser's ctor
 for more info about wxJSON extensions).

 Note that the parser has to be constructed with a flag that
 indicates if each individual wxJSON extension is on.
 If the warning message is related to an extension that is not
 enabled in the parser's \c m_flag data member, this function
 calls AddError() and the warning message becomes an error
 message.
 The \c type parameter is one of the same constants that
 specify the parser's extensions.
 If type is ZERO than the function always adds a warning
*/
void
wxJSONReader::AddWarning( int type, const wxString& msg )
{
    if ( type != 0 )    {
        if ( ( type & m_flags ) == 0 )  {
            AddError( msg );
            return;
        }
    }

    wxString err;
    err.Printf( _T( "Warning: line %d, col %d - %s"), m_lineNo, m_colNo, msg.c_str() );

    wxLogTrace( traceMask, _T("(%s) %s"), __PRETTY_FUNCTION__, err.c_str());
    if ( (int) m_warnings.size() < m_maxErrors )  {
        m_warnings.Add( err );
    }
    else if ( (int) m_warnings.size() == m_maxErrors )  {
        m_warnings.Add( _T("Error: too many warning messages - ignoring further warnings"));
    }
}

/*!
 The function reads characters from the input text
 and returns the first non-whitespace character read or -1
 if EOF.
 Note that the function does not rely on the \b isspace function
 of the C library but checks the space constants: space, TAB and
 LF.
*/
int
wxJSONReader::SkipWhiteSpace( wxInputStream& is )
{
    int ch;
    do {
        ch = ReadChar( is );
        if ( ch < 0 )  {
            break;
        }
    }
    while ( ch == ' ' || ch == '\n' || ch == '\t' );
    wxLogTrace( traceMask, _T("(%s) end whitespaces line=%d col=%d"),
             __PRETTY_FUNCTION__, m_lineNo, m_colNo );
    return ch;
}

/*!
 The function is called by DoRead() when a '/' (slash) character
 is read from the input stream assuming that a C/C++ comment is starting.
 Returns the first character that follows the comment or
 -1 on EOF.
 The function also adds a warning message because comments are not
 valid JSON text.
 The function also stores the comment, if any, in the \c m_comment data
 member: it can be used by the DoRead() function if comments have to be
 stored in the value they refer to.
*/
int
wxJSONReader::SkipComment( wxInputStream& is )
{
    static const wxChar* warn =
    _T("Comments may be tolerated in JSON text but they are not part of JSON syntax");

    int ch = ReadChar( is );
    if ( ch < 0 )  {
        return -1;
    }

    wxLogTrace( storeTraceMask, _T("(%s) start comment line=%d col=%d"),
             __PRETTY_FUNCTION__, m_lineNo, m_colNo );

    wxMemoryBuffer utf8Buff;
    unsigned char c;

    if ( ch == '/' )  {
        AddWarning( wxJSONREADER_ALLOW_COMMENTS, warn );
        m_commentLine = m_lineNo;
        utf8Buff.AppendData( "

        while ( ch >= 0 )  {
            if ( ch == '\n' )    {
                break;
            }
            if ( ch == '\r' )    {
                ch = PeekChar( is );
                if ( ch == '\n' )    {
                    ch = ReadChar( is );
                }
                break;
            }
            else    {
                c = (unsigned char) ch;
                utf8Buff.AppendByte( c );
            }
            ch = ReadChar( is );
        }
        m_comment = wxString::FromUTF8( (const char*) utf8Buff.GetData(),
                        utf8Buff.GetDataLen());
    }

    else if ( ch == '*' )  {
        AddWarning(wxJSONREADER_ALLOW_COMMENTS, warn );
        m_commentLine = m_lineNo;
        utf8Buff.AppendData( "/*", 2 );
        while ( ch >= 0 ) {
            if ( ch == '*' )    {
                ch = PeekChar( is );
                if ( ch == '/' )    {
                    (void) ReadChar( is );
                    ch = ReadChar( is );
                    utf8Buff.AppendData( "*/", 2 );
                    break;
                }
            }
            c = (unsigned char) ch;
            utf8Buff.AppendByte( c );
            ch = ReadChar( is );
        }
        if ( m_noUtf8 )    {
            m_comment = wxString::From8BitData( (const char*) utf8Buff.GetData(),
                                utf8Buff.GetDataLen());
        }
        else    {
            m_comment = wxString::FromUTF8( (const char*) utf8Buff.GetData(),
                                utf8Buff.GetDataLen());
        }
    }

    else  {
        AddError( _T( "Strange '/' (did you want to insert a comment?)"));
        while ( ch >= 0 ) {
            ch = ReadChar( is );
            if ( ch == '*' && PeekChar( is ) == '/' )  {
                break;
            }
            if ( ch == '\n' )  {
                break;
            }
        }
        ch = ReadChar( is );
    }
    wxLogTrace( traceMask, _T("(%s) end comment line=%d col=%d"),
             __PRETTY_FUNCTION__, m_lineNo, m_colNo );
    wxLogTrace( storeTraceMask, _T("(%s) end comment line=%d col=%d"),
             __PRETTY_FUNCTION__, m_lineNo, m_colNo );
    wxLogTrace( storeTraceMask, _T("(%s) comment=%s"),
             __PRETTY_FUNCTION__, m_comment.c_str());
    return ch;
}

/*!
 The function reads a string value from input stream and it is
 called by the \c DoRead() function when it enconters the
 double quote characters.
 The function read all bytes up to the next double quotes
 (unless it is escaped) and stores them in a temporary UTF-8
 memory buffer.
 Also, the function processes the escaped characters defined
 in the JSON syntax.

 Next, the function tries to convert the UTF-8 buffer to a
 \b wxString object using the \b wxString::FromUTF8 function.
 Depending on the build mode, we can have the following:
 \li in Unicode the function always succeeds, provided that the
    buffer contains valid UTF-8 code units.

 \li in ANSI builds the conversion may fail because of the presence of
    unrepresentable characters in the current locale. In this case,
    the default behaviour is to perform a char-by-char conversion; every
    char that cannot be represented in the current locale is stored as
    \e unicode \e escaped \e sequence

 \li in ANSI builds, if the reader is constructed with the wxJSONREADER_NOUTF8_STREAM
     then no conversion takes place and the UTF-8 temporary buffer is simply
     \b copied to the \b wxString object

 The string is, finally, stored in the provided wxJSONValue argument
 provided that it is empty or it contains a string value.
 This is because the parser class recognizes multi-line strings
 like the following one:
 \code
   [
      "This is a very long string value which is splitted into more"
      "than one line because it is more human readable"
   ]
 \endcode
 Because of the lack of the value separator (,) the parser
 assumes that the string was splitted into several double-quoted
 strings.
 If the value does not contain a string then an error is
 reported.
 Splitted strings cause the parser to report a warning.
*/
int
wxJSONReader::ReadString( wxInputStream& is, wxJSONValue& val )
{

    wxMemoryBuffer utf8Buff;
    char ues[8];

    int ch = 0;
    while ( ch >= 0 ) {
        ch = ReadChar( is );
        unsigned char c = (unsigned char) ch;
        if ( ch == '\\' )  {
            ch = ReadChar( is );
            switch ( ch )  {
                case -1 :
                    break;
                case 't' :
                    utf8Buff.AppendByte( '\t' );
                    break;
                case 'n' :
                    utf8Buff.AppendByte( '\n' );
                    break;
                case 'b' :
                    utf8Buff.AppendByte( '\b' );
                    break;
                case 'r' :
                    utf8Buff.AppendByte( '\r' );
                    break;
                case '\"' :
                    utf8Buff.AppendByte( '\"' );
                    break;
                case '\\' :
                    utf8Buff.AppendByte( '\\' );
                    break;
                case '/' :
                    utf8Buff.AppendByte( '/' );
                    break;
                case 'f' :
                    utf8Buff.AppendByte( '\f' );
                    break;
                case 'u' :
                    ch = ReadUES( is, ues );
                    if ( ch < 0 ) {
                        return ch;
                    }
                    AppendUES( utf8Buff, ues );
                    continue;
                default :
                    AddError( _T( "Unknow escaped character \'\\%c\'"), ch );
            }
        }
        else {
            if ( ch == '\"' )    {
                break;
            }
            utf8Buff.AppendByte( c );
        }
    }

    wxString s;
    if ( m_noUtf8 )    {
        s = wxString::From8BitData( (const char*) utf8Buff.GetData(), utf8Buff.GetDataLen());
    }
    else    {
        size_t convLen = wxConvUTF8.ToWChar( 0,
                        0,
            (const char*) utf8Buff.GetData(),
                utf8Buff.GetDataLen());

        if ( convLen == wxCONV_FAILED )    {
            AddError( _T( "String value: the UTF-8 stream is invalid"));
            s.append( _T( "<UTF-8 stream not valid>"));
        }
        else    {
#if defined( wxJSON_USE_UNICODE )
            s = wxString::FromUTF8( (const char*) utf8Buff.GetData(), utf8Buff.GetDataLen());
#else
            s = wxString::FromUTF8( (const char*) utf8Buff.GetData(), utf8Buff.GetDataLen());
            if ( s.IsEmpty() )    {
                int r = ConvertCharByChar( s, utf8Buff );
                if ( r > 0 )    {
                    AddWarning( 0, _T( "The string value contains unrepresentable Unicode characters"));
                }
            }
#endif
        }
     }
    wxLogTrace( traceMask, _T("(%s) line=%d col=%d"),
             __PRETTY_FUNCTION__, m_lineNo, m_colNo );
    wxLogTrace( traceMask, _T("(%s) string read=%s"),
             __PRETTY_FUNCTION__, s.c_str() );
    wxLogTrace( traceMask, _T("(%s) value=%s"),
             __PRETTY_FUNCTION__, val.AsString().c_str() );

    if ( !val.IsValid() )   {
        wxLogTrace( traceMask, _T("(%s) assigning the string to value"), __PRETTY_FUNCTION__ );
        val = s ;
    }
    else if ( val.IsString() )  {
        AddWarning( wxJSONREADER_MULTISTRING,
            _T("Multiline strings are not allowed by JSON syntax") );
        wxLogTrace( traceMask, _T("(%s) concatenate the string to value"), __PRETTY_FUNCTION__ );
        val.Cat( s );
    }
    else  {
        AddError( _T( "String value \'%s\' cannot follow another value"), s );
    }

    val.SetLineNo( m_lineNo );

    if ( ch >= 0 )  {
        ch = ReadChar( is );
    }
    return ch;
}

/*!
 This function is called by the ReadValue() when the
 first character encontered is not a special char
 and it is not a double-quote.
 The only possible type is a literal or a number which
 all lies in the US-ASCII charset so their UTF-8 encodeing
 is the same as US-ASCII.
 The function simply reads one byte at a time from the stream
 and appends them to a \b wxString object.
 Returns the next character read.

 A token cannot include \e unicode \e escaped \e sequences
 so this function does not try to interpret such sequences.

 @param is    the input stream
 @param ch    the character read by DoRead
 @param s    the string object that contains the token read
 @return -1 in case of errors or EOF
*/
int
wxJSONReader::ReadToken( wxInputStream& is, int ch, wxString& s )
{
    int nextCh = ch;
    while ( nextCh >= 0 ) {
        switch ( nextCh ) {
            case ' ' :
            case ',' :
            case ':' :
            case '[' :
            case ']' :
            case '{' :
            case '}' :
            case '\t' :
            case '\n' :
            case '\r' :
            case '\b' :
                wxLogTrace( traceMask, _T("(%s) line=%d col=%d"),
                     __PRETTY_FUNCTION__, m_lineNo, m_colNo );
                wxLogTrace( traceMask, _T("(%s) token read=%s"),
                     __PRETTY_FUNCTION__, s.c_str() );
                return nextCh;
                break;
            default :
                s.Append( (unsigned char) nextCh, 1 );
                break;
        }
        nextCh = ReadChar( is );
    }
    wxLogTrace( traceMask, _T("(%s) EOF on line=%d col=%d"),
         __PRETTY_FUNCTION__, m_lineNo, m_colNo );
    wxLogTrace( traceMask, _T("(%s) EOF - token read=%s"),
             __PRETTY_FUNCTION__, s.c_str() );
    return nextCh;
}

/*!
 The function is called by DoRead() when it enconters a char that is
 not a special char nor a double-quote.
 It assumes that the string is a numeric value or a literal
 boolean value and stores it in the wxJSONValue object \c val.

 The function also checks that \c val is of type wxJSONTYPE_INVALID otherwise
 an error is reported becasue a value cannot follow another value:
 maybe a (,) or (:) is missing.

 If the literal starts with a digit, a plus or minus sign, the function
 tries to interpret it as a number. The following are tried by the function,
 in this order:

 \li if the literal starts with a digit: signed integer, then unsigned integer
        and finally double conversion is tried
 \li if the literal starts with a minus sign: signed integer, then  double
        conversion is tried
 \li if the literal starts with plus sign: unsigned integer
        then double conversion is tried

 Returns the next character or -1 on EOF.
*/
int
wxJSONReader::ReadValue( wxInputStream& is, int ch, wxJSONValue& val )
{
    wxString s;
    int nextCh = ReadToken( is, ch, s );
    wxLogTrace( traceMask, _T("(%s) value=%s"),
             __PRETTY_FUNCTION__, val.AsString().c_str() );

    if ( val.IsValid() )  {
        AddError( _T( "Value \'%s\' cannot follow a value: \',\' or \':\' missing?"), s );
        return nextCh;
    }

    bool r;  double d;
#if defined( wxJSON_64BIT_INT )
    wxInt64  i64;
    wxUint64 ui64;
#else
    unsigned long int ul; long int l;
#endif

    if ( s == _T("null") ) {
        val.SetType( wxJSONTYPE_NULL );
        wxLogTrace( traceMask, _T("(%s) value = NULL"),  __PRETTY_FUNCTION__ );
        return nextCh;
    }
    else if ( s.CmpNoCase( _T( "null" )) == 0 ) {
        wxLogTrace( traceMask, _T("(%s) value = NULL"),  __PRETTY_FUNCTION__ );
        AddWarning( wxJSONREADER_CASE, _T( "the \'null\' literal must be lowercase" ));
        val.SetType( wxJSONTYPE_NULL );
        return nextCh;
    }
    else if ( s == _T("true") ) {
        wxLogTrace( traceMask, _T("(%s) value = TRUE"),  __PRETTY_FUNCTION__ );
        val = true;
        return nextCh;
    }
    else if ( s.CmpNoCase( _T( "true" )) == 0 ) {
        wxLogTrace( traceMask, _T("(%s) value = TRUE"),  __PRETTY_FUNCTION__ );
        AddWarning( wxJSONREADER_CASE, _T( "the \'true\' literal must be lowercase" ));
        val = true;
        return nextCh;
    }
    else if ( s == _T("false") ) {
        wxLogTrace( traceMask, _T("(%s) value = FALSE"),  __PRETTY_FUNCTION__ );
        val = false;
        return nextCh;
    }
    else if ( s.CmpNoCase( _T( "false" )) == 0 ) {
        wxLogTrace( traceMask, _T("(%s) value = FALSE"),  __PRETTY_FUNCTION__ );
        AddWarning( wxJSONREADER_CASE, _T( "the \'false\' literal must be lowercase" ));
        val = false;
        return nextCh;
    }


    bool tSigned = true, tUnsigned = true, tDouble = true;
    switch ( ch )  {
        case '0' :
        case '1' :
        case '2' :
        case '3' :
        case '4' :
        case '5' :
        case '6' :
        case '7' :
        case '8' :
        case '9' :
            break;

        case '+' :
            tSigned = false;
            break;

        case '-' :
            tUnsigned = false;
            break;
        default :
            AddError( _T( "Literal \'%s\' is incorrect (did you forget quotes?)"), s );
            return nextCh;
    }

    if ( tSigned )    {
    #if defined( wxJSON_64BIT_INT)
        r = Strtoll( s, &i64 );
        wxLogTrace( traceMask, _T("(%s) convert to wxInt64 result=%d"),
                  __PRETTY_FUNCTION__, r );
        if ( r )  {
            val = i64;
            return nextCh;
        }
    #else
        r = s.ToLong( &l );
        wxLogTrace( traceMask, _T("(%s) convert to int result=%d"),
                 __PRETTY_FUNCTION__, r );
        if ( r )  {
            val = (int) l;
            return nextCh;
        }
    #endif
    }

    if ( tUnsigned )    {
    #if defined( wxJSON_64BIT_INT)
        r = Strtoull( s, &ui64 );
        wxLogTrace( traceMask, _T("(%s) convert to wxUint64 result=%d"),
                              __PRETTY_FUNCTION__, r );
        if ( r )  {
            val = ui64;
            return nextCh;
        }
    #else
        r = s.ToULong( &ul );
        wxLogTrace( traceMask, _T("(%s) convert to int result=%d"),
                         __PRETTY_FUNCTION__, r );
        if ( r )  {
            val = (unsigned int) ul;
            return nextCh;
        }
    #endif
    }

    if ( tDouble )    {
        r = s.ToDouble( &d );
        wxLogTrace( traceMask, _T("(%s) convert to double result=%d"),
                 __PRETTY_FUNCTION__, r );
        if ( r )  {
            val = d;
            return nextCh;
        }
    }


    AddError( _T( "Literal \'%s\' is incorrect (did you forget quotes?)"), s );
    return nextCh;
}


/*!
 The function is called by ReadString() when the \b \\u sequence is
 encontered; the sequence introduces a control character in the form:
 \code
     \uXXXX
 \endcode
 where XXXX is a four-digit hex code..
 The function reads four chars from the input UTF8 stream by calling ReadChar()
 four times: if EOF is encontered before reading four chars, -1 is
 also returned and no sequence interpretation is performed.
 The function stores the 4 hexadecimal digits in the \c uesBuffer parameter.

 Returns the character after the hex sequence or -1 if EOF.

 \b NOTICE: although the JSON syntax states that only control characters
 are represented in this way, the wxJSON library reads and recognizes all
 unicode characters in the BMP.
*/
int
wxJSONReader::ReadUES( wxInputStream& is, char* uesBuffer )
{
    for ( int i = 0; i < 4; i++ )  {
        int ch = ReadChar( is );
        if ( ch < 0 )  {
            return ch;
        }
        uesBuffer[i] = (unsigned char) ch;
    }
    uesBuffer[4] = 0;

    return 0;
}


/*!
 This function is called by \c ReadString() when a \e unicode \e escaped
 \e sequence is read from the input text as for example:

 \code
  \u0001
 \endcode

 which represents a control character.
 The \c uesBuffer parameter contains the 4 hexadecimal digits that are
 read from \c ReadUES.

 The function tries to convert the 4 hex digits in a \b wchar_t character
 which is appended to the memory buffer \c utf8Buff after converting it
 to UTF-8.

 If the conversion from hexadecimal fails, the function does not
 store the character in the UTF-8 buffer and an error is reported.
 The function is the same in ANSI and Unicode.
 Returns -1 if the buffer does not contain valid hex digits.
 sequence. On success returns ZERO.

 @param utf8Buff    the UTF-8 buffer to which the control char is written
 @param uesBuffer    the four-hex-digits read from the input text
 @return ZERO on success, -1 if the four-hex-digit buffer cannot be converted
*/
int
wxJSONReader::AppendUES( wxMemoryBuffer& utf8Buff, const char* uesBuffer )
{
    unsigned long l;
    int r = sscanf( uesBuffer, "%lx", &l );
    if ( r != 1  )  {
        AddError( _T( "Invalid Unicode Escaped Sequence"));
        return -1;
    }
    wxLogTrace( traceMask, _T("(%s) unicode sequence=%s code=%ld"),
              __PRETTY_FUNCTION__, uesBuffer, l );

    wchar_t ch = (wchar_t) l;
    char buffer[16];
    size_t len = wxConvUTF8.FromWChar( buffer, 10, &ch, 1 );

    if ( len > 1 )    {
        len = len - 1;
    }
    utf8Buff.AppendData( buffer, len );

    wxASSERT( len != wxCONV_FAILED );
    return 0;
}

/*!
 The function searches a suitable value object for storing the
 comment line that was read by the parser and temporarly
 stored in \c m_comment.
 The function searches the three values pointed to by:
 \li \c m_next
 \li \c m_current
 \li \c m_lastStored

 The value that the comment refers to is:

 \li if the comment is on the same line as one of the values, the comment
    refer to that value and it is stored as \b inline.
 \li otherwise, if the comment flag is wxJSONREADER_COMMENTS_BEFORE, the comment lines
    are stored in the value pointed to by \c m_next
 \li otherwise, if the comment flag is wxJSONREADER_COMMENTS_AFTER, the comment lines
    are stored in the value pointed to by \c m_current or m_latStored

 Note that the comment line is only stored if the wxJSONREADER_STORE_COMMENTS
 flag was used when the parser object was constructed; otherwise, the
 function does nothing and immediatly returns.
 Also note that if the comment line has to be stored but the
 function cannot find a suitable value to add the comment line to,
 an error is reported (note: not a warning but an error).
*/
void
wxJSONReader::StoreComment( const wxJSONValue* parent )
{
    wxLogTrace( storeTraceMask, _T("(%s) m_comment=%s"),  __PRETTY_FUNCTION__, m_comment.c_str());
    wxLogTrace( storeTraceMask, _T("(%s) m_flags=%d m_commentLine=%d"),
              __PRETTY_FUNCTION__, m_flags, m_commentLine );
    wxLogTrace( storeTraceMask, _T("(%s) m_current=%p"), __PRETTY_FUNCTION__, m_current );
    wxLogTrace( storeTraceMask, _T("(%s) m_next=%p"), __PRETTY_FUNCTION__, m_next );
    wxLogTrace( storeTraceMask, _T("(%s) m_lastStored=%p"), __PRETTY_FUNCTION__, m_lastStored );

    if ( (m_flags & wxJSONREADER_STORE_COMMENTS) == 0 )  {
        m_comment.clear();
        return;
    }

    if ( m_current != 0 )  {
        wxLogTrace( storeTraceMask, _T("(%s) m_current->lineNo=%d"),
             __PRETTY_FUNCTION__, m_current->GetLineNo() );
        if ( m_current->GetLineNo() == m_commentLine ) {
            wxLogTrace( storeTraceMask, _T("(%s) comment added to \'m_current\' INLINE"),
             __PRETTY_FUNCTION__ );
            m_current->AddComment( m_comment, wxJSONVALUE_COMMENT_INLINE );
            m_comment.clear();
            return;
        }
    }
    if ( m_next != 0 )  {
        wxLogTrace( storeTraceMask, _T("(%s) m_next->lineNo=%d"),
             __PRETTY_FUNCTION__, m_next->GetLineNo() );
        if ( m_next->GetLineNo() == m_commentLine ) {
            wxLogTrace( storeTraceMask, _T("(%s) comment added to \'m_next\' INLINE"),
                 __PRETTY_FUNCTION__ );
            m_next->AddComment( m_comment, wxJSONVALUE_COMMENT_INLINE );
            m_comment.clear();
            return;
        }
    }
    if ( m_lastStored != 0 )  {
        wxLogTrace( storeTraceMask, _T("(%s) m_lastStored->lineNo=%d"),
             __PRETTY_FUNCTION__, m_lastStored->GetLineNo() );
        if ( m_lastStored->GetLineNo() == m_commentLine ) {
            wxLogTrace( storeTraceMask, _T("(%s) comment added to \'m_lastStored\' INLINE"),
                 __PRETTY_FUNCTION__ );
            m_lastStored->AddComment( m_comment, wxJSONVALUE_COMMENT_INLINE );
            m_comment.clear();
            return;
        }
    }


    if ( m_flags & wxJSONREADER_COMMENTS_AFTER )  {
        if ( m_current )  {
            if ( m_current == parent || !m_current->IsValid()) {
                AddError( _T("Cannot find a value for storing the comment (flag AFTER)"));
            }
            else  {
                wxLogTrace( storeTraceMask, _T("(%s) comment added to m_current (AFTER)"),
                     __PRETTY_FUNCTION__ );
                m_current->AddComment( m_comment, wxJSONVALUE_COMMENT_AFTER );
            }
        }
        else if ( m_lastStored )  {
            wxLogTrace( storeTraceMask, _T("(%s) comment added to m_lastStored (AFTER)"),
                 __PRETTY_FUNCTION__ );
            m_lastStored->AddComment( m_comment, wxJSONVALUE_COMMENT_AFTER );
        }
        else   {
            wxLogTrace( storeTraceMask,
                _T("(%s) cannot find a value for storing the AFTER comment"), __PRETTY_FUNCTION__ );
            AddError(_T("Cannot find a value for storing the comment (flag AFTER)"));
        }
    }
    else {
        if ( m_next )  {
            wxLogTrace( storeTraceMask, _T("(%s) comment added to m_next (BEFORE)"),
                 __PRETTY_FUNCTION__ );
            m_next->AddComment( m_comment, wxJSONVALUE_COMMENT_BEFORE );
        }
        else   {
            AddError(_T("Cannot find a value for storing the comment (flag BEFORE)"));
        }
    }
    m_comment.clear();
}


/*!
 This function returns the number of bytes that represent a unicode
 code point in various encoding.
 For example, if the input stream is UTF-32 the function returns 4.
 Because the only recognized format for streams is UTF-8 the function
 just calls UTF8NumBytes() and returns.
 The function is, actually, not used at all.

*/
int
wxJSONReader::NumBytes( char ch )
{
    int n = UTF8NumBytes( ch );
    return n;
}

/*!
 The function counts the number of '1' bit in the character \c ch and
 returns it.
 The UTF-8 encoding specifies the number of bytes needed by a wide character
 by coding it in the first byte. See below.

 Note that if the character does not contain a valid UTF-8 encoding
 the function returns -1.

\code
   UCS-4 range (hex.)    UTF-8 octet sequence (binary)
   -------------------   -----------------------------
   0000 0000-0000 007F   0xxxxxxx
   0000 0080-0000 07FF   110xxxxx 10xxxxxx
   0000 0800-0000 FFFF   1110xxxx 10xxxxxx 10xxxxxx
   0001 0000-001F FFFF   11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
   0020 0000-03FF FFFF   111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
   0400 0000-7FFF FFFF   1111110x 10xxxxxx ... 10xxxxxx
\endcode
*/
int
wxJSONReader::UTF8NumBytes( char ch )
{
    int num = 0;
    for ( int i = 0; i < 8; i++ )  {
        if ( (ch & 0x80) == 0 )  {
            break;
        }
        ++num;
        ch = ch << 1;
    }

    if ( num > 6 )  {
        num = -1;
    }
    else if ( num == 0 )  {
        num = 1;
    }
    return num;
}

/*!
 This function is used in ANSI mode when input from a stream is in UTF-8
 format and the UTF-8 buffer read cannot be converted to the locale
 wxString object.
 The function performs a char-by-char conversion of the buffer and appends
 every representable character to the string \c s.
 Characters that cannot be represented are stored as \e unicode \e escaped
 \e sequences in the form:
 \code
   \uXXXX
 \endcode
 where XXXX is a for-hex-digits Unicode code point.
 The function returns the number of characters that cannot be represented
 in the current locale.
*/
int
wxJSONReader::ConvertCharByChar( wxString& s, const wxMemoryBuffer& utf8Buffer )
{
    size_t len  = utf8Buffer.GetDataLen();
    char*  buff = (char*) utf8Buffer.GetData();
    char* buffEnd = buff + len;

    int result = 0;
    char temp[16];

    while ( buff < buffEnd )    {
        temp[0] = *buff;
        int numBytes = NumBytes( *buff );
        ++buff;
        for ( int i = 1; i < numBytes; i++ )    {
            if ( buff >= buffEnd )    {
                break;
            }
            temp[i] = *buff;
            ++buff;
        }
        wchar_t dst[10];
        size_t outLength = wxConvUTF8.ToWChar( dst, 10, temp, numBytes );

        len = wxConvLibc.FromWChar( temp, 16, dst, outLength );
        if ( len == wxCONV_FAILED )    {
            ++result;
            wxString t;
            t.Printf( _T( "\\u%04X"), (int) dst[0] );
            s.Append( t );
        }
        else    {
            s.Append( temp[0], 1 );
        }
    }
    return result;
}

/*!
 This function is called by DoRead() when the single-quote character is
 encontered which starts a \e memory \e buffer type.
 This type is a \b wxJSON extension so the function emits a warning
 when such a type encontered.
 If the reader is constructed without the \c wxJSONREADER_MEMORYBUFF flag
 then the warning becomes an error.
 To know more about this JSON syntax extension read \ref wxjson_tutorial_memorybuff

 @param is the input stream
 @param val the JSON value that will hold the memory buffer value
 @return the last char read or -1 in case of EOF
*/

int
wxJSONReader::ReadMemoryBuff( wxInputStream& is, wxJSONValue& val )
{
    static const wxChar* membuffError = _T("the \'memory buffer\' type contains %d invalid digits" );

    AddWarning( wxJSONREADER_MEMORYBUFF, _T( "the \'memory buffer\' type is not valid JSON text" ));

    wxMemoryBuffer buff;
    int ch = 0; int errors = 0;
    unsigned char byte;
    while ( ch >= 0 ) {
        ch = ReadChar( is );
        if ( ch < 0 )  {
            break;
        }
        if ( ch == '\'' )  {
            break;
        }
        unsigned char c1 = (unsigned char) ch;
        ch = ReadChar( is );
        if ( ch < 0 )  {
            break;
        }
        unsigned char c2 = (unsigned char) ch;
        c1 -= '0';
        c2 -= '0';
        if ( c1 > 9 )  {
            c1 -= 7;
        }
        if ( c2 > 9 )  {
            c2 -= 7;
        }
        if ( c1 > 15 )  {
            ++errors;
        }
        else if ( c2 > 15 )  {
            ++errors;
        }
        else {
            byte = (c1 * 16) + c2;
            buff.AppendByte( byte );
        }
    }

    if ( errors > 0 )  {
        wxString err;
        err.Printf( membuffError, errors );
        AddError( err );
    }


    if ( !val.IsValid() )   {
        wxLogTrace( traceMask, _T("(%s) assigning the memory buffer to value"), __PRETTY_FUNCTION__ );
        val = buff ;
    }
    else if ( val.IsMemoryBuff() )  {
        wxLogTrace( traceMask, _T("(%s) concatenate memory buffer to value"), __PRETTY_FUNCTION__ );
        val.Cat( buff );
    }
    else  {
        AddError( _T( "Memory buffer value cannot follow another value") );
    }

    val.SetLineNo( m_lineNo );

    if ( ch >= 0 )  {
        ch = ReadChar( is );
    }
    return ch;
}




#if defined( wxJSON_64BIT_INT )
/*!
 This function implements a simple variant
 of the \b strtoll C-library function.
 I needed this implementation because the wxString::To(U)LongLong
 function does not work on my system:

  \li GNU/Linux Fedora Core 6
  \li GCC version 4.1.1
  \li libc.so.6

 The wxWidgets library (actually I have installed version 2.8.7)
 relies on \b strtoll in order to do the conversion from a string
 to a long long integer but, in fact, it does not work because
 the 'wxHAS_STRTOLL' macro is not defined on my system.
 The problem only affects the Unicode builds while it seems
 that the wxString::To(U)LongLong function works in ANSI builds.

 Note that this implementation is not a complete substitute of the
 strtoll function because it only converts decimal strings (only base
 10 is implemented).

 @param str the string that contains the decimal literal
 @param i64 the pointer to long long which holds the converted value

 @return TRUE if the conversion succeeds
*/
bool
wxJSONReader::Strtoll( const wxString& str, wxInt64* i64 )
{
    wxChar sign = ' ';
    wxUint64 ui64;
    bool r = DoStrto_ll( str, &ui64, &sign );

    if ( r) {
        switch ( sign )  {
            case '-' :
                if ( ui64 > (wxUint64) LLONG_MAX + 1 )  {
                    r = false;
                }
                else  {
                    *i64 = (wxInt64) (ui64 * -1);
                }
                break;

            default :
                if ( ui64 > LLONG_MAX )  {
                    r = false;
                }
                else  {
                    *i64 = (wxInt64) ui64;
                }
                break;
        }
    }
    return r;
}


/*!
 Similar to \c Strtoll but for unsigned integers
*/
bool
wxJSONReader::Strtoull( const wxString& str, wxUint64* ui64 )
{
    wxChar sign = ' ';
    bool r = DoStrto_ll( str, ui64, &sign );
    if ( sign == '-' )  {
        r = false;
    }
    return r;
}

/*!
 This function is called internally by the \c Strtoll and \c Strtoull functions
 and it does the actual conversion.
 The function is also able to check numeric overflow.

 @param str the string that has to be converted
 @param ui64 the pointer to a unsigned long long that holds the converted value
 @param sign the pointer to a wxChar character that will get the sign of the literal string, if any
 @return TRUE if the conversion succeeds
*/
bool
wxJSONReader::DoStrto_ll( const wxString& str, wxUint64* ui64, wxChar* sign )
{

    int maxDigits = 20;

    wxUint64 power10[] = {
    wxULL(1),
    wxULL(10),
    wxULL(100),
    wxULL(1000),
    wxULL(10000),
    wxULL(100000),
    wxULL(1000000),
    wxULL(10000000),
    wxULL(100000000),
    wxULL(1000000000),
    wxULL(10000000000),
    wxULL(100000000000),
    wxULL(1000000000000),
    wxULL(10000000000000),
    wxULL(100000000000000),
    wxULL(1000000000000000),
    wxULL(10000000000000000),
    wxULL(100000000000000000),
    wxULL(1000000000000000000),
    wxULL(10000000000000000000)
  };


    wxUint64 temp1 = wxULL(0);

    int strLen = str.length();
    if ( strLen == 0 )  {
        *ui64 = wxLL(0);
        return true;
    }

    int index = 0;
    wxChar ch = str[0];
    if ( ch == '+' || ch == '-' )  {
        *sign = ch;
        ++index;
        ++maxDigits;
    }

    if ( strLen > maxDigits )  {
        return false;
    }

    if ( strLen == maxDigits )  {
        wxString uLongMax( _T("18446744073709551615"));
        int j = 0;
        for ( int i = index; i < strLen - 1; i++ )  {
            ch = str[i];
            if ( ch < '0' || ch > '9' ) {
                return false;
            }
            if ( ch > uLongMax[j] ) {
                return false;
            }
            if ( ch < uLongMax[j] ) {
                break;
            }
            ++j;
        }
    }

    int exponent = 0;
    for ( int i = strLen - 1; i >= index; i-- )   {
        wxChar ch = str[i];
        if ( ch < '0' || ch > '9' ) {
            return false;
        }
        ch = ch - '0';
        temp1 += ch * power10[exponent];
        ++exponent;
    }
    *ui64 = temp1;
    return true;
}

#endif

/*
{
}
*/

