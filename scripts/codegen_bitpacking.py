#!/usr/bin/env python3
"""
AVX2 Bit-Packing/Unpacking Code Generator
Generates pack, unpack, masked pack, and masked unpack functions
Supports u16, u32, and u64 data types
"""


# Configuration for each data type
DATA_TYPES = {
    "u16": {
        "c_type": "uint16_t",
        "element_bits": 16,
        "elements_per_vector": 16,  # __m256i holds 16 uint16_t values
        "avx_type": "__m256i",
        "shift_left": "_mm256_slli_epi16",
        "shift_right": "_mm256_srli_epi16",
        "set1": "_mm256_set1_epi16",
        "max_bits": 16,
    },
    "u32": {
        "c_type": "uint32_t",
        "element_bits": 32,
        "elements_per_vector": 8,  # __m256i holds 8 uint32_t values
        "avx_type": "__m256i",
        "shift_left": "_mm256_slli_epi32",
        "shift_right": "_mm256_srli_epi32",
        "set1": "_mm256_set1_epi32",
        "max_bits": 32,
    },
    "u64": {
        "c_type": "uint64_t",
        "element_bits": 64,
        "elements_per_vector": 4,  # __m256i holds 4 uint64_t values
        "avx_type": "__m256i",
        "shift_left": "_mm256_slli_epi64",
        "shift_right": "_mm256_srli_epi64",
        "set1": "_mm256_set1_epi64x",
        "max_bits": 64,
    },
}


def generate_pack_function(bits, use_mask=False, dtype="u32"):
    """Generate a single avxpackblock function for a given bit width and data type"""

    config = DATA_TYPES[dtype]
    c_type = config["c_type"]
    element_bits = config["element_bits"]
    elements_per_vector = config["elements_per_vector"]
    shift_left = config["shift_left"]
    set1 = config["set1"]

    # Total elements to pack is always 256
    total_elements = 256
    num_vectors = total_elements // elements_per_vector

    func_name = (
        f"avxpackblockmask{bits}_{dtype}" if use_mask else f"avxpackblock{bits}_{dtype}"
    )

    if bits == 0:
        return f"""static void {func_name}(const {c_type} *pin, __m256i *compressed)
                {{
                    (void)compressed;
                    (void)pin; /* we consumed {total_elements} {element_bits}-bit integers */
                }}
                """

    total_bits = total_elements * bits
    output_words = (total_bits + 255) // 256

    lines = []
    lines.append(f"static void {func_name}(const {c_type} *pin, __m256i *compressed)")
    lines.append("{")

    # Check if we need tmp variable
    needs_tmp = False
    for i in range(num_vectors):
        start_bit = i * bits * elements_per_vector
        end_bit = start_bit + bits * elements_per_vector
        if start_bit // 32 != (end_bit - 1) // 32:
            needs_tmp = True
            break

    lines.append(f"    /* we are going to touch  {output_words} 256-bit words */")
    lines.append("    __m256i w0, w1;")
    lines.append("    const __m256i *in = (const __m256i *)pin;")

    # Add mask for masked version
    if use_mask:
        mask_value = (1 << bits) - 1
        lines.append(f"    const __m256i mask = {set1}({mask_value});")

    if needs_tmp:
        lines.append("    __m256i tmp; /* used to store inputs at word boundary */")

    # Track state
    out_word_idx = 0

    for vec_idx in range(num_vectors):
        start_bit = vec_idx * bits * elements_per_vector
        end_bit = start_bit + bits * elements_per_vector
        start_word = start_bit // 32
        end_word = (end_bit - 1) // 32
        offset_in_word = start_bit % 32
        word_var = "w0" if out_word_idx % 2 == 0 else "w1"

        def load_expr(idx):
            load = f"_mm256_lddqu_si256(in + {idx})"
            if use_mask:
                load = f"_mm256_and_si256(mask, {load})"
            return load

        if start_word == end_word:
            if (
                offset_in_word == 0
                and vec_idx % ((256 // (bits * elements_per_vector)) if bits > 0 else 1)
                == 0
            ):
                lines.append(f"    {word_var} = {load_expr(vec_idx)};")
            else:
                shift_expr = f"{shift_left}({load_expr(vec_idx)}, {offset_in_word})"
                lines.append(
                    f"    {word_var} = _mm256_or_si256({word_var}, {shift_expr});"
                )

            if end_bit % 32 == 0:
                lines.append(
                    f"    _mm256_storeu_si256(compressed + {out_word_idx}, {word_var});"
                )
                out_word_idx += 1
        else:
            bits_in_first = 32 - offset_in_word
            lines.append(f"    tmp = {load_expr(vec_idx)};")
            lines.append(
                f"    {word_var} = _mm256_or_si256({word_var}, {shift_left}(tmp, {offset_in_word}));"
            )
            next_word_var = "w1" if word_var == "w0" else "w0"
            lines.append(
                f"    {next_word_var} = _mm256_srli_epi32(tmp, {bits_in_first});"
            )
            lines.append(
                f"    _mm256_storeu_si256(compressed + {out_word_idx}, {word_var});"
            )
            out_word_idx += 1

    final_bit = num_vectors * bits * elements_per_vector
    if final_bit % 32 != 0:
        final_word_var = "w0" if (out_word_idx % 2) == 0 else "w1"
        lines.append(
            f"    _mm256_storeu_si256(compressed + {out_word_idx}, {final_word_var});"
        )

    lines.append("}")
    return "\n".join(lines)


def generate_unpack_function(bits, dtype="u32"):
    """Generate a single avxunpackblock function for a given bit width and data type"""

    config = DATA_TYPES[dtype]
    c_type = config["c_type"]
    element_bits = config["element_bits"]
    elements_per_vector = config["elements_per_vector"]
    shift_left = config["shift_left"]
    shift_right = config["shift_right"]
    set1 = config["set1"]

    total_elements = 256
    num_vectors = total_elements // elements_per_vector

    if bits == 0:
        return f"""static void avxunpackblock0_{dtype}(const __m256i *compressed, {c_type} *pout)
                {{
                    (void)compressed;
                    memset(pout, 0, {total_elements} * sizeof({c_type}));
                }}
                """

    total_bits = total_elements * bits
    input_words = (total_bits + 255) // 256
    total_bytes = input_words * 32
    mask_value = (1 << bits) - 1

    lines = []
    lines.append(
        f"/* we packed {total_elements} {bits}-bit values, touching {input_words} 256-bit words, using {total_bytes} bytes */"
    )
    lines.append(
        f"static void avxunpackblock{bits}_{dtype}(const __m256i *compressed, {c_type} *pout)"
    )
    lines.append("{")
    lines.append(f"    /* we are going to access  {input_words} 256-bit words */")
    lines.append("    __m256i w0, w1;")
    lines.append("    __m256i *out = (__m256i *)pout;")
    lines.append(f"    const __m256i mask = {set1}({mask_value});")

    # Track which input word we're reading from
    current_input_word = 0
    current_word_var = "w0"

    # Load first word
    lines.append(f"    {current_word_var} = _mm256_lddqu_si256(compressed);")

    for out_vec_idx in range(num_vectors):
        start_bit = out_vec_idx * bits * elements_per_vector
        end_bit = start_bit + bits * elements_per_vector

        start_word = start_bit // 32
        end_word = (end_bit - 1) // 32
        offset_in_word = start_bit % 32

        # Load new word if needed
        if start_word > current_input_word:
            current_input_word = start_word
            current_word_var = "w1" if current_word_var == "w0" else "w0"
            lines.append(
                f"    {current_word_var} = _mm256_lddqu_si256(compressed + {current_input_word});"
            )

        if start_word == end_word:
            # Value doesn't cross word boundary
            if offset_in_word == 0:
                # At word start - just mask
                if end_bit % 32 == 0:  # Last value in word, no mask needed
                    lines.append(
                        f"    _mm256_storeu_si256(out + {out_vec_idx}, {current_word_var});"
                    )
                else:
                    lines.append(
                        f"    _mm256_storeu_si256(out + {out_vec_idx}, _mm256_and_si256(mask, {current_word_var}));"
                    )
            else:
                # Shift right and mask
                if (
                    offset_in_word + bits * elements_per_vector == 32
                ):  # Last value, no mask needed
                    lines.append(
                        f"    _mm256_storeu_si256(out + {out_vec_idx}, {shift_right}({current_word_var}, {offset_in_word}));"
                    )
                else:
                    lines.append(
                        f"    _mm256_storeu_si256(out + {out_vec_idx}, _mm256_and_si256(mask, {shift_right}({current_word_var}, {offset_in_word})));"
                    )
        else:
            # Value crosses word boundary - need to OR two parts
            bits_in_first = 32 - offset_in_word
            bits_in_second = bits * elements_per_vector - bits_in_first

            next_word_var = "w1" if current_word_var == "w0" else "w0"

            # Need to load next word if we haven't already
            if end_word > current_input_word:
                current_input_word = end_word
                lines.append(
                    f"    {next_word_var} = _mm256_lddqu_si256(compressed + {current_input_word});"
                )

            or_expr = f"_mm256_or_si256({shift_right}({current_word_var}, {offset_in_word}), {shift_left}({next_word_var}, {bits_in_first}))"
            lines.append(
                f"    _mm256_storeu_si256(out + {out_vec_idx}, _mm256_and_si256(mask, {or_expr}));"
            )

            current_word_var = next_word_var

    lines.append("}")
    return "\n".join(lines)


def generate_dispatch_arrays(dtype="u32"):
    """Generate function pointer arrays for a specific data type"""

    config = DATA_TYPES[dtype]
    c_type = config["c_type"]
    max_bits = config["max_bits"]

    lines = []

    # Pack functions array
    lines.append(f"/* Pack function pointer arrays for {dtype} */")
    lines.append(f"typedef void (*avx_pack_func_{dtype}_t)(const {c_type}*, __m256i*);")
    lines.append("")
    lines.append(
        f"static const avx_pack_func_{dtype}_t avx_pack_functions_{dtype}[] = {{"
    )
    for bits in range(max_bits + 1):
        lines.append(f"    avxpackblock{bits}_{dtype},")
    lines.append("};")
    lines.append("")

    # Pack mask functions array
    lines.append(
        f"static const avx_pack_func_{dtype}_t avx_pack_mask_functions_{dtype}[] = {{"
    )
    for bits in range(max_bits + 1):
        lines.append(f"    avxpackblockmask{bits}_{dtype},")
    lines.append("};")
    lines.append("")

    # Unpack functions array
    lines.append(f"/* Unpack function pointer arrays for {dtype} */")
    lines.append(
        f"typedef void (*avx_unpack_func_{dtype}_t)(const __m256i*, {c_type}*);"
    )
    lines.append("")
    lines.append(
        f"static const avx_unpack_func_{dtype}_t avx_unpack_functions_{dtype}[] = {{"
    )
    for bits in range(max_bits + 1):
        lines.append(f"    avxunpackblock{bits}_{dtype},")
    lines.append("};")

    return "\n".join(lines)


def generate_wrapper_functions(dtype="u32"):
    """Generate high-level wrapper functions for pack/unpack operations"""

    config = DATA_TYPES[dtype]
    c_type = config["c_type"]
    max_bits = config["max_bits"]

    lines = []
    lines.append(f"// High-level wrapper functions for {dtype}")
    lines.append("// " + "-" * 70 + "\n")

    # Define function specifications
    functions = [
        {
            "name": f"AvxPack_{dtype}",
            "comment": 'reads 256 values from "in", writes "bit" 256-bit vectors to "out"',
            "signature": f"static inline {c_type}* AvxPack_{dtype}(const {c_type} *in, __m256i *out, const uint32_t number, const uint32_t bit)",
            "array": f"avx_pack_mask_functions_{dtype}",
            "loop_body": ["func(in + i * 256, out);", "out += bit;"],
            "return": f"return ({c_type}*)out;",
        },
        {
            "name": f"AvxPackWithoutMask_{dtype}",
            "comment": 'reads 256 values from "in", writes "bit" 256-bit vectors to "out"',
            "signature": f"static inline {c_type}* AvxPackWithoutMask_{dtype}(const {c_type} *in, __m256i *out, const uint32_t number, const uint32_t bit)",
            "array": f"avx_pack_functions_{dtype}",
            "loop_body": ["func(in + i * 256, out);", "out += bit;"],
            "return": f"return ({c_type}*)out;",
        },
        {
            "name": f"AvxUnpack_{dtype}",
            "comment": 'reads "bit" 256-bit vectors from "in", writes 256 values to "out"',
            "signature": f"static inline {c_type}* AvxUnpack_{dtype}(const __m256i *in, {c_type} *out, const uint32_t number, const uint32_t bit)",
            "array": f"avx_unpack_functions_{dtype}",
            "loop_body": ["func(in, out);", "in += bit;", "out += 256;"],
            "return": "return out;",
        },
    ]

    # Generate each function
    for func_spec in functions:
        lines.append(f"/* {func_spec['comment']} */")
        lines.append(f"/* must contain at least 256 items */")
        lines.append(func_spec["signature"])
        lines.append("{")
        lines.append(f"    if (bit > {max_bits}) {func_spec['return']}")
        lines.append(f"    auto func = {func_spec['array']}[bit];")
        lines.append("    for (uint32_t i = 0; i < number / 256; ++i) {")
        for body_line in func_spec["loop_body"]:
            lines.append(f"        {body_line}")
        lines.append("    }")
        lines.append(f"    {func_spec['return']}")
        lines.append("}\n")

    return "\n".join(lines)


def main():
    """Generate the complete header file"""

    with open("avx_bitpacking_generated.hpp", "w") as f:
        f.write("#ifndef AVX_BITPACKING_GENERATED_H\n")
        f.write("#define AVX_BITPACKING_GENERATED_H\n\n")
        f.write("/* AUTO-GENERATED CODE - DO NOT EDIT */\n")
        f.write("/* Generated by avx_bitpacking_generator.py */\n")
        f.write("/* Supports u16, u32, and u64 data types */\n\n")
        f.write("#include <immintrin.h>\n")
        f.write("#include <stdint.h>\n")
        f.write("#include <string.h>\n\n")

        for dtype in ["u16", "u32", "u64"]:
            config = DATA_TYPES[dtype]
            max_bits = config["max_bits"]

            f.write("// " + "=" * 70 + "\n")
            f.write(f"// {dtype.upper()} Functions\n")
            f.write("// " + "=" * 70 + "\n\n")

            # Generate regular pack functions
            f.write(f"// Regular Pack Functions for {dtype} (no masking)\n")
            f.write("// " + "-" * 70 + "\n\n")
            for bits in range(max_bits + 1):
                f.write(generate_pack_function(bits, use_mask=False, dtype=dtype))
                f.write("\n\n")

            # Generate masked pack functions
            f.write(f"// Masked Pack Functions for {dtype} (with bit masking)\n")
            f.write("// " + "-" * 70 + "\n\n")
            for bits in range(max_bits + 1):
                f.write(generate_pack_function(bits, use_mask=True, dtype=dtype))
                f.write("\n\n")

            # Generate unpack functions
            f.write(f"// Unpack Functions for {dtype}\n")
            f.write("// " + "-" * 70 + "\n\n")
            for bits in range(max_bits + 1):
                f.write(generate_unpack_function(bits, dtype=dtype))
                f.write("\n\n")

            # Generate function pointer arrays
            f.write(f"// Function Pointer Arrays for {dtype}\n")
            f.write("// " + "-" * 70 + "\n\n")
            f.write(generate_dispatch_arrays(dtype))
            f.write("\n\n")

            # Generate wrapper functions
            f.write(f"// Wrapper Functions for {dtype}\n")
            f.write("// " + "-" * 70 + "\n\n")
            f.write(generate_wrapper_functions(dtype))
            f.write("\n\n")

        f.write("#endif /* AVX_BITPACKING_GENERATED_H */\n")

    print("✓ Generated avx_bitpacking_generated.hpp")
    print("\nGenerated functions for each data type:")
    for dtype in ["u16", "u32", "u64"]:
        config = DATA_TYPES[dtype]
        max_bits = config["max_bits"]
        num_funcs = max_bits + 1
        print(f"\n  {dtype.upper()}:")
        print(f"    - {num_funcs} regular pack functions (0-{max_bits} bits)")
        print(f"    - {num_funcs} masked pack functions (0-{max_bits} bits)")
        print(f"    - {num_funcs} unpack functions (0-{max_bits} bits)")
        print(f"    - Function pointer arrays")
        print(f"    - 3 high-level wrapper functions")


if __name__ == "__main__":
    main()
