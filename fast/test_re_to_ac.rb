#!/usr/bin/env ruby

$:.unshift(File.dirname(__FILE__))
require 're_to_ac'
require 'set'

def canonical(result)
  result.collect {|x| x.to_set}.to_set
end
def fmt(result)
  result.collect {|x| '(' + x.to_a.join(' AND ') + ')'}.join(' OR ')
end

[
  # Repetition Tests
  [ 'trivial',        [['trivial']]                                         ],
  [ 'x{10}',          [['xxxxxxxxxx']]                                      ],
  [ 'x{5,10}',        [['xxxxx']]                                           ],
  [ 'a?x{5}c?',       [['xxxxx']]                                           ],
  [ 'a?x{5}c?y{5}d?', [['xxxxx', 'yyyyy']]                                  ],
  [ 'x{5}y{5}',       [['xxxxxyyyyy']]                                      ],
  [ 'x{5,10}y{5}',    [['xxxxxyyyyy']]                                      ],
  [ 'x{5}y{5,10}',    [['xxxxxyyyyy']]                                      ],
  [ 'a{3}b{2,3}c{3}', [['aaabb', 'bbccc']]                                  ],
  [ '(a?foo)(barb?)', [['foobar']]                                          ],

  # Alternation Tests
  [ 'foo|bar',         [['foo'], ['bar']]                                   ],
  [ '(foo|bar){2}',    [['foobar'], ['foofoo'], ['barbar'], ['barfoo']]     ],
  [ 'a(foox?|barx?)y', [['afoo', 'y'], ['abar', 'y']]                       ],
  [ 'a(foox?|bar)y',   [['afoo', 'y'], ['abary']]                           ],
  [ 'a(foo|barx?)y',   [['afooy'], ['abar', 'y']]                           ],

  # Meta
  [ '.',               [['\.']]                                             ],

  # Case Insensitive
  [ '(?i)foo',         [['\if\io\io']]                                      ],
  [ 'foo(?i:bar)baz',  [['foo\ib\ia\irbaz']]                                ],
  [ 'foo(?i)bar',      [['foo\ib\ia\ir']]                                   ],
  [ '(?i)foo(?-i)bar', [['\if\io\iobar']]                                   ],
  [ '(?i)foo(?-i:bar)',[['\if\io\iobar']]                                   ],


  # Escape
  [ '\\\\\e\f\n\r\t\v',   [['\\\\\e\f\n\r\t\v']]                            ],
  [ '\.\|\$\^',           [['.|$^']]                                        ],
  [ '\*\?\+\(\)\[\]\{\}', [['*?+()\[\]{}']]                                 ],
  [ '\a',                 [['\^G']]                                         ],
  # Following fails due to regexp_parser bug.
#  [ '\cD\c?\c;',          [['\^D\^?\x7b']]                                  ],
  [ '\cD',                [['\^D']]                                         ],
  [ '\xaa',               [['\xaa']]                                        ],

  # Simple character sets
  [ '\d\D\s\S\w\W',       [['\d\D\s\S\w\W']]                                ],

  # Anchors
  [ '^\b\B\A\Z\z\G$',     [[]]                                              ],

  # Unions
  [ '[abc]',              [['[abc]']]                                       ],
  [ '[a-z]',              [['[a-z]']]                                       ],
  [ '[^a-z]',             [['[^a-z]']]                                      ],

  # Assertions
  [ '(?<=foo)bar(?=baz)', [['foobarbaz']]                                   ],
  [ '(?<=foo)bar(?!baz)', [['foobar']]                                      ],
  [ '(?<!foo)bar(?=baz)', [['barbaz']]                                      ],
  [ '(?<!foo)bar(?!baz)', [['bar']]                                         ],

  # Regression
  [ '\d{10}',             [['\d\d\d\d\d\d\d\d\d\d']]                        ]
].each do |re, expected|
  actual = canonical(ReToAC::extract(re))
  expected = canonical(expected)
  if actual != expected
    puts "#{re} FAIL"
    puts "  expected: #{fmt(expected)}"
    puts "  actual:   #{fmt(actual)}"
  else
    puts "#{re} PASS"
  end
end
