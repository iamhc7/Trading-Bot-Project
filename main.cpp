#include <iostream>
#include <curl/curl.h>
#include <string>
#include <nlohmann/json.hpp>
#include <deque>

const std::string API_KEY = "K2YDRFNRWNFX2BW5"; // Replace with your API key
const std::string SYMBOL = "AAPL";// Indicates the stock that you are interested in to find the price.
using json = nlohmann::json;

// Callback function is defined such that the chunks of data received from the API server can be handled. 
// Contents points to the address of the chunks of data, size represents the size (in bytes) of each unit of data in the response.
// Nmemb represents the number of pieces of data and Userp is a pointer which directs to a variable where the API's resoponse will be stored.
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
// Userp is redeclared as a string and then the member access operator is used to call the append method.Which is then used to add data based on the pointer and total size of the entire chunk.
    return size * nmemb; // Required for CURL to check the total amout of data processed.
}

// Function to calculate the Exponential Moving Average (EMA)
double calculateEMA(double currentPrice, double previousEMA, double smoothingConstant) {
    return (currentPrice * smoothingConstant) + (previousEMA * (1 - smoothingConstant));
}

// Function to check trading signals based on EMA crossover
void checkTradingSignal(double shortEMA, double longEMA) {
    static bool isPositionOpen = false;  // Track if we have an open position
// When shortEMA increases, it typically means a rise in price due to short smoothing constant which makes shortEMA more responsive to price changes
// When shortEMA > longEMA, it suggests a bullish trend and prices are likely to continue increasing. Hence, a buy option is called.
    if (shortEMA > longEMA && !isPositionOpen) {
        std::cout << "Buy Signal!" << std::endl;
        isPositionOpen = true;  // Open position (buy)
    }
    else if (shortEMA < longEMA && isPositionOpen) {
        std::cout << "Sell Signal!" << std::endl;
        isPositionOpen = false;  // Close position (sell)
    }
}

int main() {
    CURL* curl;
    CURLcode res;
    std::string response_string;
    std::string url = "https://www.alphavantage.co/query?function=TIME_SERIES_INTRADAY&symbol=" + SYMBOL + "&interval=5min&apikey=" + API_KEY;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();// Intialises the CURL session

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str()); // Configures the curl request, sets the URL to fetch data from
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback); // Provides the function to handle the incoming data
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string); // Address-of Operator is used such that CURL can pass the data to the response_string variable
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);  // Debugging

        std::cout << "Sending request to Alpha Vantage..." << std::endl;
        res = curl_easy_perform(curl); // Sends the request to Alpha Vantage API

        if (res != CURLE_OK) {
            std::cerr << "cURL Error: " << curl_easy_strerror(res) << std::endl; // If the HTTP request was unsucessful, an error message is printed
        } else {
            try {
                json j = json::parse(response_string);  // Parse the response into a JSON object

                // Check if the "Time Series (5min)" key exists in the response
                if (j.contains("Time Series (5min)")) {
                    auto time_series = j["Time Series (5min)"]; // Accesses the values associated with the key

                    // Store closing prices for EMA calculation
                    std::deque<double> closePrices; // Double-ended queue is used which allows for methods such as .pop_back() to be used
                    double shortEMA = 0.0, longEMA = 0.0;
                    double shortSmoothingConstant = 2.0 / (5 + 1);  // Short-term EMA smoothing constant (5-period EMA)
                    double longSmoothingConstant = 2.0 / (20 + 1);  // Long-term EMA smoothing constant (20-period EMA)

                    // Iterate through the time series
                    for (auto& entry : time_series.items()) {
                        std::string timestamp = entry.key();
                        double closePrice = std::stod(entry.value()["4. close"].get<std::string>());
                        
                        closePrices.push_front(closePrice);  // Add the close price to the front (latest data first)

                        // Maintain only the last 20 data points
                        if (closePrices.size() > 20) {
                            closePrices.pop_back();  // Remove the oldest data
                        }

                        // Calculate short and long EMA when we have enough data
                        if (closePrices.size() >= 5) {
                            if (shortEMA == 0.0) shortEMA = closePrice;  // Initial EMA for short-term
                            if (longEMA == 0.0) longEMA = closePrice;  // Initial EMA for long-term

                            // Calculate the EMAs
                            shortEMA = calculateEMA(closePrice, shortEMA, shortSmoothingConstant);
                            longEMA = calculateEMA(closePrice, longEMA, longSmoothingConstant);

                            // Check for trading signal based on EMA crossover
                            checkTradingSignal(shortEMA, longEMA);

                            // Output the data and EMAs
                            std::cout << "Time: " << timestamp << std::endl;
                            std::cout << "Close Price: " << closePrice << std::endl;
                            std::cout << "Short EMA (5): " << shortEMA << std::endl;
                            std::cout << "Long EMA (20): " << longEMA << std::endl;
                        }
                    }
                } else {
                    std::cout << "No time series data found in the response." << std::endl;
                }

            } catch (const json::parse_error& e) {
                std::cerr << "Error parsing JSON: " << e.what() << std::endl;
            }
        }

        curl_easy_cleanup(curl);
    } else {
        std::cerr << "Failed to initialize cURL." << std::endl;
    }

    curl_global_cleanup();
    return 0;
}

