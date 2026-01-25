// Copyright (c) 2010-2015 Sami Väisänen, Ensisoft 
//
// http://www.ensisoft.com
// 
// This software is copyrighted software. Unauthorized hacking, cracking, distribution
// and general assing around is prohibited.
// Redistribution and use in source and binary forms, with or without modification,
// without permission are prohibited.
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#pragma once

#include <newsflash/config.h>

#include <newsflash/warnpush.h>
#  include <boost/spirit/include/classic.hpp>
#include <newsflash/warnpop.h>

#include <string>
#include <iterator>
#include <cstdint>

// yEnc decoder/encoder implementation.
// http://www.yenc.org/

namespace yenc
{
    // yEnc header precedes the yEnc encoded data.
    // In case of a multipart data the part field will 
    // be present and have a non-zero value after successful parsing.
    struct Header {
        uint64_t part = 0;            // the number of the part when the yenc content is a part of a multipart binary. (not available in single part binaries)
        uint64_t line = 0;            // typical line length.
        uint64_t size = 0;            // the size of the original binary (in bytes)
        uint64_t total = 0;           // the total number of parts in a multipart binary. (only available in yEnc 1.2)
        std::string name;       // the filename of the encoded binary
    };

    // yEnc part header. 
    // ypart specifies the starting and ending points (in bytes) of the block
    // in the original file    
    struct PartHeader {
        uint64_t begin = 0; // starting point
        uint64_t end = 0;   // ending point
    };

    // yEnd end line follows the yEnc encoded data.
    // In case of a multipart data the part field will
    // be present and have a non-zero value after successful parsing.
    struct Footer {
        uint64_t size = 0;
        uint64_t part = 0;
        uint32_t crc32 = 0;
        uint32_t pcrc32 = 0;
    };
    

    // parse yEnc header. returns {true, header} if successful, otherwise {false, header} and
    // contents of header are unspecified.
    template<typename InputIterator>
    std::pair<bool, Header> ParseHeader(InputIterator& beg, InputIterator end)
    {
        using namespace boost::spirit::classic;
        
        Header header;

        const uint_parser<uint64_t> uint64_p  = uint_parser<uint64_t>();
        const chset<> name_p(chset<>(anychar_p) - '\r' - '\n');

        const auto ret = parse(beg, end,
           (
            str_p("=ybegin") >>
            !(str_p("part=")  >> uint_p[assign(header.part)])    >> // not included in single part headers
            !(str_p("total=") >> uint_p[assign(header.total)])   >> // only in yEnc 1.2
            ((str_p("line=")  >> uint_p[assign(header.line)]) | (str_p("size=")  >> uint64_p[assign(header.size)])) >>
            ((str_p("line=")  >> uint_p[assign(header.line)]) | (str_p("size=")  >> uint64_p[assign(header.size)])) >>
            (str_p("name=")  >> (*name_p)[assign(header.name)])   >>
            !eol_p
            ), ch_p(' '));
        
        beg = ret.stop;

        return {ret.hit, header};
    }

    // parse yEnc part. returns { true, part } if successful, otherwise { false, part  } and
    // the contents of the part are unspecified.
    template<typename InputIterator>
    std::pair<bool, PartHeader> ParsePart(InputIterator& beg, InputIterator end)
    {
        using namespace boost::spirit::classic;

        PartHeader part;

        const uint_parser<uint64_t> uint64_p  = uint_parser<uint64_t>();

        const auto ret = parse(beg, end, 
          (
           str_p("=ypart")  >>
           (str_p("begin=") >> uint64_p[assign(part.begin)]) >>
           (str_p("end=")   >> uint64_p[assign(part.end)]) >>
           !eol_p
           ), ch_p(' '));

        beg = ret.stop;

        return {ret.hit, part};
    }


    template<typename InputIterator>
    std::pair<bool, Footer> ParseFooter(InputIterator& beg, InputIterator end)
    {
        using namespace boost::spirit::classic;
        
        Footer footer;

        const uint_parser<uint64_t> uint64_p  = uint_parser<uint64_t>();

        const auto ret = parse(beg, end, 
          (                                                
           str_p("=yend") >>
           (str_p("size=") >> uint64_p[assign(footer.size)]) >>
           !(str_p("part=") >> uint64_p[assign(footer.part)]) >>
           !(str_p("pcrc32=") >> hex_p[assign(footer.pcrc32)]) >>
           !(str_p("crc32=") >> hex_p[assign(footer.crc32)]) >> 
           !eol_p
          ), space_p);

        beg = ret.stop;

        return {ret.hit, footer};
    }

    // Decode yEnc encoded data back into its original binary representation.
    template<typename InputIterator, typename OutputIterator>
    bool Decode(InputIterator& beg, InputIterator end, OutputIterator dest)
    {
        // todo: some error checking perhaps?
        while (beg != end)
        {
            unsigned char c = *beg;
            if( c == '\r' || c == '\n') 
            {
                ++beg;
                continue;
            }
            else if (c == '=')
            {
                // todo: this might not actually work for an InputIterator
                InputIterator tmp(beg);
                if (++beg == end)
                {
                    *dest++ = c-42;
                    return true;
                }
                else if (*beg == 'y')
                {
                    beg = tmp;
                    return true;
                }
                c  = *beg;
                c -= 64;
            }
            *dest++ = c - 42;
            ++beg;
        }
        return true;
    }

    // Encode data into yEnc. Line should be the preferred
    // line length after wich a new line (\r\n) is written into the output stream.
    template<typename InputIterator, typename OutputIterator>
    void Encode(InputIterator beg, InputIterator end, OutputIterator dest, const int line = 128, bool double_dots = false)
    {
        int output = 0;
        while (beg != end)
        {
            int val = (*beg + 42) % 256;
            // these are characters that may not appear in 
            // yEnc encoded content without being "escaped".
            if (val == 0x00 || val == 0x0A || val == 0x0D || val == 0x3D || val == 0x09)
            {
                output++;
                *dest++ = '=';
                val = (val + 64) % 256;
            }
            *dest++ = static_cast<char>(val);
            output++;
            if (double_dots && output == 1)
            {
                if (static_cast<char>(val) == '.')
                    *dest++ = '.';
            } 
            else if (output >= line)
            {
                *dest++ = '\r';
                *dest++ = '\n';
                output = 0;
            }
            ++beg;
        }
    }

} // yEnc

