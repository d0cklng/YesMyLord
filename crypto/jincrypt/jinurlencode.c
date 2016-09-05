/* ************************************
 *
 * urlencode 编解码
 * 也叫百分号编码 当前标准：2005年1月发布的RFC 3986
 * RFC 3986 section 2.2 保留字符 (2005年1月)
 * !	*	'	(	)	;	:	@	&	=	+	$	,	/	?	#	[	]
 * RFC 3986 section 2.3 未保留字符 (2005年1月)
 * A	B	C	D	E	F	G	H	I	J	K	L	M	N	O	P	Q	R	S	T	U	V	W	X	Y	Z
 * a	b	c	d	e	f	g	h	i	j	k	l	m	n	o	p	q	r	s	t	u	v	w	x	y	z
 * 0	1	2	3	4	5	6	7	8	9	-	_	.	~
 * 编码后%xx的形式是大小写都OK的
 * 在特定上下文中没有特殊含义的保留字符也可以被百分号编码，在语义上与不百分号编码的该字符没有差别.
 * 在URI的"查询"成分(?字符后的部分)中, 例如"/"仍然是保留字符但是没有特殊含义，除非一个特定的URI有其它规定. 该/字符在没有特殊含义时不需要百分号编码.
 * 如果保留字符具有特殊含义，那么该保留字符用>百分号编码的URI与该保留字符仅用其自身表示的URI具有不同的语义。
 *
 * 保留字符需要编码，二进制非ASCII需要转到UTF8后编码，但是也有不规矩的编码出现..
 * 未保留字符不需要百分号编码.但是有的编码器这么做了..
 * MIME类型是application/x-www-form-urlencoded的编码非常古老，可能把空格编码为‘+’而不是我们以为的‘%20’
 *
 *    The following are two example URIs and their component parts:

         foo://example.com:8042/over/there?name=ferret#nose
         \_/   \______________/\_________/ \_________/ \__/
          |           |            |            |        |
       scheme     authority       path        query   fragment
          |   _____________________|__
         / \ /                        \
         urn:example:animal:ferret:nose
 *
 * ************************************/



