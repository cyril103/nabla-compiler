if exists("b:current_syntax")
  finish
endif

syntax keyword nablaKeyword def class new import if else match while for val var this
syntax keyword nablaBoolean true false
syntax keyword nablaType Int Long Float Double Bool Char String Unit
syntax keyword nablaType IntArray LongArray FloatArray DoubleArray BoolArray
syntax keyword nablaType Array ArrayInt ArrayLong ArrayFloat ArrayDouble ArrayBool ArrayObject Option

syntax match nablaLong "\v<\d+L>"
syntax match nablaFloat "\v<\d+\.\d+f>"
syntax match nablaDouble "\v<\d+\.\d+>"
syntax match nablaNumber "\v<\d+>"

syntax region nablaString start=/"/ skip=/\\"/ end=/"/
syntax region nablaChar start=/'/ skip=/\\'/ end=/'/

syntax match nablaOperator "=>"
syntax match nablaOperator "==\|!=\|<=\|>=\|&&\|||\|[+\-*/<>!=]"
syntax match nablaDelimiter "[(){}\[\],.:]"
syntax match nablaComment "//.*$"

highlight default link nablaKeyword Keyword
highlight default link nablaBoolean Boolean
highlight default link nablaType Type
highlight default link nablaNumber Number
highlight default link nablaLong Number
highlight default link nablaFloat Float
highlight default link nablaDouble Float
highlight default link nablaString String
highlight default link nablaChar Character
highlight default link nablaOperator Operator
highlight default link nablaDelimiter Delimiter
highlight default link nablaComment Comment

let b:current_syntax = "nabla"
