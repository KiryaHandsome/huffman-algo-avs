#include <iostream>
#include <vector>
#include <map>
#include <list>
#include <fstream>
#include <thread>
#include <mutex>

using namespace std;

class Node {
public:
    int frequency = 0;
    char symbol = 0;
    Node *left, *right;

    Node() {
        left = right = nullptr;
    }

    Node(Node *left, Node *right) {
        this->left = left;
        this->right = right;
        frequency = left->frequency + right->frequency;
    }
};


struct MyCompare {
    bool operator()(const Node *l, const Node *r) const {
        return l->frequency < r->frequency;
    }
};

vector<char> code;
map<char, vector<char>> dictionary;
mutex mtx;
map<char, int> symbolsTable;

void buildDictionary(Node *root) {
    if (root->left != nullptr) {
        code.push_back(0);
        buildDictionary(root->left);
    }

    if (root->right != nullptr) {
        code.push_back(1);
        buildDictionary(root->right);
    }

    if (root->left == nullptr && root->right == nullptr)
        dictionary[root->symbol] = code;

    if (!code.empty())
        code.pop_back();
}

void buildDictionaryLog(Node *root) {
    auto start = chrono::high_resolution_clock::now();
    buildDictionary(root);
    auto stop = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::microseconds>(stop - start);
    cout << "Build dictionary: " << duration.count() << " microseconds" << endl;
}

Node *buildHuffmanTreeThread(list<Node *> &t) {
    while (!t.empty()) {
        t.sort(MyCompare());

        Node *sonLeft = t.front();
        t.pop_front();
        Node *sonRight = t.front();
        t.pop_front();

        Node *parent = new Node(sonLeft, sonRight);
        t.push_back(parent);
    }
    return t.front();
}

void printDecoded(Node *root, string encodedDataFilePath) {
    ifstream encodedDataFile(encodedDataFilePath, ios::in | ios::binary);
    setlocale(LC_ALL, "Russian");
    Node *currentNode = root;
    int count = 0;
    char data;
    encodedDataFile.read(&data, 1);
    while (encodedDataFile.gcount() > 0) {
        bool b = data & (1 << (7 - count));
        if (b) {
            currentNode = currentNode->right;
        } else {
            currentNode = currentNode->left;
        }
        if (currentNode->left == nullptr && currentNode->right == nullptr) {
            cout << currentNode->symbol;
            currentNode = root;
        }
        count++;
        if (count == 8) {
            count = 0;
            encodedDataFile.read(&data, 1);
        }
    }
    cout << "\n";

    encodedDataFile.close();
}

void writeEncodedToFile(ifstream &inputFile, string outputFilePath) {
    inputFile.clear();
    inputFile.seekg(0);
    ofstream fileToOutput(outputFilePath, ios::out | ios::binary);
    int count = 0;
    char buf = 0;
    while (!inputFile.eof()) {
        char c = inputFile.get();
        vector<char> x = dictionary[c];
        for (const char &n: x) {
            buf = buf | n << (7 - count);
            count++;
            if (count == 8) {
                count = 0;
                fileToOutput << buf;
                buf = 0;
            }
        }
    }
    fileToOutput.close();
}

void writeEncodedToFileLog(ifstream &inputFile, string outputFilePath) {
    auto start = chrono::high_resolution_clock::now();
    writeEncodedToFile(inputFile, outputFilePath);
    auto stop = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::microseconds>(stop - start);
    cout << "Write to encoded file: " << duration.count() << " microseconds" << endl;
}

Node *buildTree(list<Node *> treeNodes) {
    while (treeNodes.size() != 1) {
        treeNodes.sort(MyCompare());

        Node *leftSon = treeNodes.front();
        treeNodes.pop_front();
        Node *rightSon = treeNodes.front();
        treeNodes.pop_front();

        Node *parent = new Node(leftSon, rightSon);
        treeNodes.push_back(parent);
    }
    return treeNodes.front();
}

Node *buildTreeLog(list<Node *> treeNodes) {
    auto buildTreeStart = chrono::high_resolution_clock::now();
    Node *root = buildTreeSequential(treeNodes);
    auto buildTreeStop = chrono::high_resolution_clock::now();
    auto buildTreeDuration = chrono::duration_cast<chrono::microseconds>(buildTreeStop - buildTreeStart);
    cout << "Build tree: " << buildTreeDuration.count() << " microseconds" << endl;
    return root;
}

void countSymbols(const string &chunk) {
    map<char, int> localTable;

    for (char current: chunk) {
        localTable[current]++;
    }

    for (const auto &entry: localTable) {
        mtx.lock();
        symbolsTable[entry.first] += entry.second;
        mtx.unlock();
    }
}

list<Node *> createTreeNodes() {
    list<Node *> treeNodes;
    for (auto &itr: symbolsTable) {
        Node *node = new Node;
        node->symbol = itr.first;
        node->frequency = itr.second;
        treeNodes.push_back(node);
    }
    return treeNodes;
}

void readFromFileSequential(ifstream &inputFile) {
    while (!inputFile.eof()) {
        char current = inputFile.get();
        symbolsTable[current]++;
    }
}

void readFromFileSequentialLog(ifstream &inputFile) {
    auto start = chrono::high_resolution_clock::now();
    readFromFileSequential(inputFile);
    auto stop = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::microseconds>(stop - start);
    cout << "Read from file sequential: " << duration.count() << " microseconds" << endl;
}

void readFromFileParallel(ifstream &inputFile, const int numThreads) {
    inputFile.seekg(0, ios::end);
    streampos fileSize = inputFile.tellg();
    inputFile.seekg(0, ios::beg);

    vector<thread> threads;

    // Read the file in chunks and process each chunk in a separate thread
    const streampos chunkSize = fileSize / numThreads;
    for (int i = 0; i < numThreads; ++i) {
        string chunk(chunkSize, '\0');
        inputFile.read(&chunk[0], chunkSize);
        threads.emplace_back(countSymbols, chunk);
    }

    // Wait for all threads to finish
    for (auto &thread: threads) {
        thread.join();
    }

    // Process the remaining data (if any) in the main thread
    string remainingChunk(fileSize % numThreads, '\0');
    inputFile.read(&remainingChunk[0], remainingChunk.size());
    countSymbols(remainingChunk);
}

void readFromFileParallelLog(ifstream &inputFile, const int numThreads) {
    auto start = chrono::high_resolution_clock::now();
    readFromFileParallel(inputFile, numThreads);
    auto stop = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::microseconds>(stop - start);
    cout << "Create symbols dictionary: " << duration.count() << " microseconds" << endl;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        cout << argc << "\n";
        for(int i = 0; i < argc; i++)
            cout << argv[i] << " ";
        cout << "\n";
        cout << "Arguments format: <threads number> <path to input file> <path to output file>\n";
        return 1;
    }
    const int numThreads = std::stoi(argv[1]);
    const string inputPath = argv[2];
    const string outputPath = argv[3];

    ifstream inputFile(inputPath, ios::out | ios::binary);
    if (!inputFile.is_open()) {
        cout << "File not opened";
        return 1;
    }

//    readFromFileSequentialLog(inputFile);
    readFromFileParallelLog(inputFile, numThreads);
    list<Node *> treeNodes = createTreeNodes();
    Node *root = buildTreeSequentialLog(treeNodes);
    buildDictionaryLog(root);
    writeEncodedToFileLog(inputFile, outputPath);
    inputFile.close();
//    printDecoded(root, "/media/kirya/linux/projects/AVS/output.txt");
    return 0;
}