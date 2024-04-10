#include <database_controller.h>
#include "filesystem"

void conversionTest() {

    ConverterToSpirv converter{ "C:\\VulkanSDK\\1.3.261.1\\Bin\\glslc" };
    converter.convertAllApplicableShaders("C:\\Users\\Viqtop\\source\\repos\\vulkan-guide\\vulkan-base\\shaders",
        "C:\\Users\\Viqtop\\source\\repos\\vulkan-guide\\vulkan-build\\shaders");
}

void test()
{
    /*
    try
    {
        int i, fld;
        time_t tmStart, tmEnd;
        CppSQLiteDB db;

        cout << "SQLite Version: " << db.SQLiteVersion() << endl;

        db.open(gszFile);
        cout << db.execScalar("select count(*) from emp;") 
               << " rows in emp table in ";
        db.Close();
    }
    catch (CppSQLiteException& e)
    {
        cerr << e.errorCode() << ":" << e.errorMessage() << endl;
    }*/
}

void ConverterToSpirv::convertShaderToSpirv(std::string sourceFilePath, std::string resultFilePath) {
    system((glslFilePath + " " + sourceFilePath + " -o " + resultFilePath + ".spv").c_str());
}

void ConverterToSpirv::convertAllApplicableShaders(std::string sourceFolderPath, std::string resultFolderPath) {
    for (const std::filesystem::directory_entry& item : std::filesystem::directory_iterator(sourceFolderPath))
        for (const std::string str : applicableExtensions)
            if (item.path().string().compare(item.path().string().size() - str.size(), str.size(), str) == 0)
                convertShaderToSpirv(item.path().string(), resultFolderPath + "\\" + item.path().filename().string());
}

