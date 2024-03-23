#include "common.hpp"
#include "assembler.hpp"
#include "global.hpp"

std::unordered_map<std::string, std::string> parseArgs(int argc, const char* argv[]) {
    std::unordered_map<std::string, std::string> args;
    if (argc < 2) {
        Porkchop::Error error;
        error.with(Porkchop::ErrorMessage().fatal().text("too few arguments, input file expected"));
        error.with(Porkchop::ErrorMessage().usage().text("Porkchop <input> [options...]"));
        error.report(nullptr, true);
        std::exit(10);
    }
    args["input"] = argv[1];
    for (int i = 2; i < argc; ++i) {
        if (!strcmp("-o", argv[i])) {
            args["output"] = argv[++i];
        } else if (!strcmp("-m", argv[i]) || !strcmp("--mermaid", argv[i])) {
            args["type"] = "mermaid";
        } else if (!strcmp("-l", argv[i]) || !strcmp("--llvm-ir", argv[i])) {
            args["type"] = "llvm-ir";
        } else if (!strcmp("-g", argv[i]) || !strcmp("--debug", argv[i])) {
            Porkchop::Assembler::debug_flag = true;
        } else {
            Porkchop::Error().with(
                    Porkchop::ErrorMessage().fatal().text("unknown flag: ").text(argv[i])
                    ).report(nullptr, false);
            std::exit(11);
        }
    }
    if (!args.contains("type")) {
        Porkchop::Error().with(
                Porkchop::ErrorMessage().fatal().text("output type is not specified")
                ).report(nullptr, false);
        std::exit(12);
    }
    if (!args.contains("output")) {
        auto const& input = args["input"];
        args["output"] = input.substr(0, input.find_last_of('.')) + '.' + args["type"];
    }
    return args;
}

struct OutputFile {
    FILE* file;

    explicit OutputFile(std::string const& filename, bool bin) {
        if (filename == "<null>") {
            file = nullptr;
        } else if (filename == "<stdout>") {
            file = stdout;
        } else {
            file = Porkchop::open(filename.c_str(), bin ? "wb" : "w");
        }
    }

    ~OutputFile() {
        if (file != nullptr && file != stdout) {
            fclose(file);
        }
    }

    void puts(const char* str) const {
        if (file != nullptr)
            fputs(str, file);
    }

    void write(Porkchop::Assembler* assembler) const {
        if (file != nullptr)
            assembler->write(file);
    }
};

int main(int argc, const char* argv[]) try {
    Porkchop::forceUTF8();
    auto args = parseArgs(argc, argv);
    auto path = fs::absolute(fs::path(args["input"]));
    auto original = Porkchop::readText(Porkchop::open(path.c_str(), "r"));
    Porkchop::Source source;
    Porkchop::tokenize(source, original);
    Porkchop::GlobalScope global(path);
    Porkchop::Compiler compiler(&global, std::move(source));
    Porkchop::parse(compiler);
    auto const& output_type = args["type"];
    OutputFile output_file(args["output"], output_type == "bin-asm");
    if (output_type == "mermaid") {
        auto descriptor = compiler.walkDescriptor();
        output_file.puts(descriptor.c_str());
    } else if (output_type == "llvm-ir") {
        Porkchop::Assembler assembler;
        assembler.init_debug(path.filename(), path.parent_path());
        try {
            compiler.compile(&assembler);
        } catch (Porkchop::Error& e) {
            e.report(&compiler.source, true);
            std::exit(-1);
        }
        output_file.write(&assembler);
    }
    puts("Compilation is done successfully");
} catch (std::bad_alloc& e) {
    fprintf(stderr, "Compiler out of memory\n");
    std::exit(-10);
} catch (std::exception& e) {
    fprintf(stderr, "Internal Compiler Error: %s\n", e.what());
    std::exit(-100);
}
