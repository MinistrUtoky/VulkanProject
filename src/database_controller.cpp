#include <database_controller.h>
using namespace std;

void DataController::test()
{
    try {
        sqlite3* db;
        if (sqlite3_open(dataAddress, &db) != SQLITE_OK) {
            printf("ERROR: can't open database: %s\n", sqlite3_errmsg(db));
            sqlite3_close(db);
        }
        else {
            fprintf(stderr, "Opened database successfully\n");

        }
        sqlite3_close(db);
    }
    catch (exception e) {
        cerr << e.what() << endl;
    }
}

void write_blob_into_file(char* filePath, vector<unsigned char> blob) {
    if (std::filesystem::exists(filePath)) {
        printf("OVERWRITING EXISTING FILE!!!\n");
    }
    std::ofstream file(filePath, std::ios::out | std::ios::binary);
    std::ostream_iterator<unsigned char> iter(file);
    copy(blob.cbegin(), blob.cend(), iter);
}


void DataController::retrieve_blobs(const char* writeDirectory, const char* tableName, int filenameColumnIndex, int blobColumnIndex) {
    try {
        sqlite3* db;
        if (sqlite3_open_v2(dataAddress, &db, (SQLITE_OPEN_READWRITE | SQLITE_OPEN_URI), NULL) != SQLITE_OK) {
            printf("Can't open DB: %s\n", sqlite3_errmsg(db));
        }
        else {
            printf("Opened %s\n", sqlite3_filename_database(sqlite3_db_filename(db, nullptr)));
            std::string sqlcommand;
            sqlite3_stmt* statement;
            sqlcommand = "SELECT * FROM "; sqlcommand.append(tableName); sqlcommand.append(";");
            printf("COMMAND: %s\n", sqlcommand.c_str());
            if (sqlite3_prepare_v2(db, sqlcommand.c_str(), -1, &statement, NULL) != SQLITE_OK) {
                printf("Sql command went wrong: %s\n", sqlite3_errmsg(db));
                sqlite3_close(db);
                sqlite3_finalize(statement);
                return;
            }
            int ret_code = 0;
            std::string filePath;
            while ((ret_code = sqlite3_step(statement)) == SQLITE_ROW) {
                printf("FILENAME = %s\n", (char*)sqlite3_column_text(statement, filenameColumnIndex));
                int size = sqlite3_column_bytes(statement, blobColumnIndex);
                unsigned char* p = (unsigned char*)sqlite3_column_blob(statement, blobColumnIndex);
                vector<unsigned char> data(p, p + size);
                filePath = writeDirectory;
                filePath.append((char*)sqlite3_column_text(statement, filenameColumnIndex));
                printf("FILEPATH = %s\n", filePath.c_str());
                write_blob_into_file((char*)filePath.c_str(), data);
            }
            if (ret_code != SQLITE_DONE) {
                printf("Table retrieval went wrong: %s\n", sqlite3_errmsg(db));
                printf("ret_code = %d\n", ret_code);
            }
            sqlite3_finalize(statement);
        }
        sqlite3_close(db);
    }
    catch (exception e) {
        cerr << e.what() << endl;
    }
}

void DataController::retrieve_model_blobs() {
    retrieve_blobs(writeModelsFileAdress, "models", 1, 2);
}

void DataController::retrieve_shader_blobs() {
    retrieve_blobs(writeShadersFileAdress, "shaders", 1, 2);
}

void DataController::retrieve_all_blobs() {
    retrieve_model_blobs();
    retrieve_shader_blobs();
    converter.convertAllApplicableShaders("..\\shaders", "..\\shaders");
}

void ConverterToSpirv::convertShaderToSpirv(std::string sourceFilePath, std::string resultFilePath) {
    printf("Converting %s to %s.spv\n", sourceFilePath.c_str(), resultFilePath.c_str());
    system((glslFilePath + " " + sourceFilePath + " -o " + resultFilePath + ".spv").c_str());
}

void ConverterToSpirv::convertAllApplicableShaders(std::string sourceFolderPath, std::string resultFolderPath) {
    for (const std::filesystem::directory_entry& item : std::filesystem::directory_iterator(sourceFolderPath))
        for (const std::string ext : applicableExtensions) {
            if (item.path().extension().string() == ext)
                convertShaderToSpirv(item.path().string(), resultFolderPath + "\\" + item.path().filename().string());
        }
}

