using ZCM
using juliazcm.types: example_t

fields = fieldnames(example_t)
consts = constfieldnames(example_t)

println("example_t")
println("  fieldnames : ")
for f = fields
    println("    $f : $(fieldtype(example_t, f))")
end
println("  constfieldnames : ")
for c = consts
    println("    $c : $(fieldtype(example_t, c))")
end
