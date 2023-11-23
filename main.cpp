#include <iostream>
#include <vector>
#include <map>
#include <list>
#include <fstream>
#include <thread>
#include <mutex>
#include <unistd.h>

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
bool isLoading = false;
pthread_mutex_t readMutex;
map<char, int> symbolsTable;

void animation() {
    const int sleepFor = 500'000;
    while (true) {
        while (isLoading) {
            usleep(sleepFor);
            std::cout << "\b\\" << std::flush;
            usleep(sleepFor);
            std::cout << "\b|" << std::flush;
            usleep(sleepFor);
            std::cout << "\b/" << std::flush;
            usleep(sleepFor);
            std::cout << "\b-" << std::flush;
        }
    }
}

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
    cout << "Building dictionary: ";
    isLoading = true;
    buildDictionary(root);
    auto stop = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::microseconds>(stop - start);
    isLoading = false;
    cout << "\b" << duration.count() << " microseconds" << endl;
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
    cout << "Writing encoded to file: ";
    isLoading = true;
    writeEncodedToFile(inputFile, outputFilePath);
    auto stop = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::microseconds>(stop - start);\
    isLoading = false;
    cout << "\b" << duration.count() << " microseconds" << endl;
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
    cout << "Building huffman tree: ";
    isLoading = true;
    Node *root = buildTree(treeNodes);
    auto buildTreeStop = chrono::high_resolution_clock::now();
    auto buildTreeDuration = chrono::duration_cast<chrono::microseconds>(buildTreeStop - buildTreeStart);
    isLoading = false;
    cout << "\b" << buildTreeDuration.count() << " microseconds" << endl;
    return root;
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

// Function to be executed by each thread
void *countSymbols(void *arg) {
    string &chunk = *static_cast<string *>(arg);
    map<char, int> localTable;

    for (char current: chunk) {
        localTable[current]++;
    }

    // Use mutex to protect the shared symbolsTable
    for (const auto &entry: localTable) {
        pthread_mutex_lock(&readMutex);
        symbolsTable[entry.first] += entry.second;
        pthread_mutex_unlock(&readMutex);
    }

    return nullptr;
}

void readFromFile(ifstream &inputFile, const int numThreads) {
    inputFile.seekg(0, ios::end);
    streampos fileSize = inputFile.tellg();
    inputFile.seekg(0, ios::beg);

    vector<pthread_t> threads;
    vector<string> chunks(numThreads);

    // Read the file in chunks and process each chunk in a separate thread
    const streampos chunkSize = fileSize / numThreads;
    for (int i = 0; i < numThreads; ++i) {
        chunks[i].resize(chunkSize);
        inputFile.read(&chunks[i][0], chunkSize);
        pthread_t thread;
        pthread_create(&thread, nullptr, countSymbols, &chunks[i]);
        threads.push_back(thread);
    }

    // Wait for all threads to finish
    for (pthread_t &thread: threads) {
        pthread_join(thread, nullptr);
    }

    // Process the remaining data (if any) in the main thread
    string remainingChunk(fileSize % numThreads, '\0');
    if (remainingChunk.size() != 0) {
        inputFile.read(&remainingChunk[0], remainingChunk.size());
        countSymbols(&remainingChunk[0]);
    }
}

void readFromFileLog(ifstream &inputFile, const int numThreads) {
    auto start = chrono::high_resolution_clock::now();
    cout << "Read from file: ";
    isLoading = true;
    readFromFile(inputFile, numThreads);
    auto stop = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::microseconds>(stop - start);
    isLoading = false;
    cout << "\b" << duration.count() << " microseconds" << endl;
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        cout << argc << "\n";
        for (int i = 0; i < argc; i++)
            cout << argv[i] << " ";
        cout << "\n";
        cout << "Arguments format: <threads number> <path to input file> <path to output file> "
                "<y if print result to stdout, no otherwise>\n";
        return 1;
    }
    pthread_mutex_init(&readMutex, nullptr);

    const int numThreads = std::stoi(argv[1]);
    const string inputPath = argv[2];
    const string outputPath = argv[3];
    const string needPrint = argv[4];
    bool print;
    if (needPrint == "y") {
        print = true;
    } else {
        print = false;
    }

    thread anim(animation);
    anim.detach();

    ifstream inputFile(inputPath, ios::out | ios::binary);
    if (!inputFile.is_open()) {
        cout << "File not opened";
        return 1;
    }

    readFromFileLog(inputFile, numThreads);
    list<Node *> treeNodes = createTreeNodes();
    Node *root = buildTreeLog(treeNodes);
    buildDictionaryLog(root);
    writeEncodedToFileLog(inputFile, outputPath);
    inputFile.close();
    if (print) {
        printDecoded(root, "/media/kirya/linux/projects/AVS/output.txt");
    }
    return 0;
}