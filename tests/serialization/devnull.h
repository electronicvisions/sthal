#pragma once

template<typename Ch, typename Traits = std::char_traits<Ch> >
struct basic_nullbuf : std::basic_streambuf<Ch, Traits>
{
     typedef std::basic_streambuf<Ch, Traits> base_type;
     typedef typename base_type::int_type int_type;
     typedef typename base_type::traits_type traits_type;

     virtual inline
	 int_type overflow(int_type c) {
         return traits_type::not_eof(c);
     }
};

typedef basic_nullbuf<char> nullbuf;
