using ZCM
using juliazcm.types: example_t

println("Fieldnames:")
println(fieldnames(example_t))
println("ConstFieldnames:")
println(constfieldnames(example_t))
