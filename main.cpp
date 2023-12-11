#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using filesystem::path;

path operator""_p(const char* data, std::size_t sz) {
    return path(data, data + sz);
}

bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories);

bool FindInVector(const string& m, const path& out_file, const vector<path>& include_directories) {
    path other_file_from_vector;
    for (auto file : include_directories) {
        other_file_from_vector = file / m;
        if (Preprocess(other_file_from_vector, out_file, include_directories)) {
            return true;
        }
    }
    return false;
}

bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories) {
    ifstream input_file(in_file, ios::in);

    if (!input_file) {
        return false;
    }

    if (filesystem::exists(in_file) && filesystem::status(in_file).type() == filesystem::file_type::regular) {
        ofstream output_file(out_file, ios::app);
        string str;
        static regex reg_in_parent_folder(R"/(\s*#\s*include\s*"([^"]*)"\s*)/");// #include "..."
        static regex reg_in_vector(R"/(\s*#\s*include\s*<([^>]*)>\s*)/");// #include <...>
        smatch m;
        size_t line = 0;

        while (getline(input_file, str)) {
            ++line;
            if (regex_match(str, m, reg_in_parent_folder)) {
                path other_file_in_parent_folder = in_file.parent_path() / string(m[1]);

                if (!Preprocess(other_file_in_parent_folder, out_file, include_directories)) {
                    if (!FindInVector(string(m[1]), out_file, include_directories)) {
                        cout << "unknown include file " << string(m[1]) << " at file " << in_file.string() << " at line " << line << endl;
                        return false;
                    }
                }
            }
            else if (regex_match(str, m, reg_in_vector)) {
                if (!FindInVector(string(m[1]), out_file, include_directories)) {
                    cout << "unknown include file " << string(m[1]) << " at file " << in_file.string() << " at line " << line << endl;
                    return false;
                }
            }
            else {
                output_file << str << endl;
            }
        }
        return true;
    }
    else if (filesystem::status(in_file).type() == filesystem::file_type::directory) {
        for (const auto& dir_entry : filesystem::directory_iterator(in_file)) {
            Preprocess(dir_entry, out_file, include_directories);
        }
    }
    return true;
}

string GetFileContents(string file) {
    ifstream stream(file);
    // конструируем string по двум итераторам
    return { (istreambuf_iterator<char>(stream)), istreambuf_iterator<char>() };
}

void Test() {
    error_code err;
    filesystem::remove_all("sources"_p, err);
    filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
    filesystem::create_directories("sources"_p / "include1"_p, err);
    filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

    {
        ofstream file("sources/a.cpp");
        file << "// this comment before include\n"
            "#include \"dir1/b.h\"\n"
            "// text between b.h and c.h\n"
            "#include \"dir1/d.h\"\n"
            "\n"
            "int SayHello() {\n"
            "    cout << \"hello, world!\" << endl;\n"
            "#   include<dummy.txt>\n"
            "}\n"s;
    }
    {
        ofstream file("sources/dir1/b.h");
        file << "// text from b.h before include\n"
            "#include \"subdir/c.h\"\n"
            "// text from b.h after include"s;
    }
    {
        ofstream file("sources/dir1/subdir/c.h");
        file << "// text from c.h before include\n"
            "#include <std1.h>\n"
            "// text from c.h after include\n"s;
    }
    {
        ofstream file("sources/dir1/d.h");
        file << "// text from d.h before include\n"
            "#include \"lib/std2.h\"\n"
            "// text from d.h after include\n"s;
    }
    {
        ofstream file("sources/include1/std1.h");
        file << "// std1\n"s;
    }
    {
        ofstream file("sources/include2/lib/std2.h");
        file << "// std2\n"s;
    }

    assert((!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
        { "sources"_p / "include1"_p,"sources"_p / "include2"_p })));

    ostringstream test_out;
    test_out << "// this comment before include\n"
        "// text from b.h before include\n"
        "// text from c.h before include\n"
        "// std1\n"
        "// text from c.h after include\n"
        "// text from b.h after include\n"
        "// text between b.h and c.h\n"
        "// text from d.h before include\n"
        "// std2\n"
        "// text from d.h after include\n"
        "\n"
        "int SayHello() {\n"
        "    cout << \"hello, world!\" << endl;\n"s;

    assert(GetFileContents("sources/a.in"s) == test_out.str());
}

int main() {
    Test();
}