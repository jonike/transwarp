#include "statistical_key_facts.h"
#include "../src/transwarp.h"
#include <fstream>
#include <iostream>
#include <random>
#include <numeric>

namespace tw = transwarp;

namespace {

using data_t = std::shared_ptr<std::vector<double>>;

// cppcheck-suppress passedByValue
data_t generate_gamma(std::size_t sample_size, double alpha, double beta, std::shared_ptr<std::mt19937> gen) {
    auto data = std::make_shared<std::vector<double>>(sample_size);
    std::gamma_distribution<double> dist(alpha, beta);
    for (auto& value : *data) {
        value = dist(*gen);
    }
    return data;
}

double average(data_t data) {
    return std::accumulate(data->begin(), data->end(), 0.) / static_cast<double>(data->size());
}

double stddev(data_t data, double average) {
    double sum = 0;
    std::for_each(data->begin(), data->end(),
                  [average,&sum](double x) { sum += std::pow(x - average, 2.); });
    return std::sqrt(sum / static_cast<double>(data->size()));
}

double median(data_t data) {
    auto copy = *data;
    std::sort(copy.begin(), copy.end());
    if(data->size() % 2 == 0)
        return (copy[data->size() / 2 - 1] + copy[data->size() / 2]) / 2;
    else
        return copy[data->size() / 2];
}

int mode(data_t data) {
    auto copy = *data;
    std::sort(copy.begin(), copy.end());
    int number = static_cast<int>(*copy.begin());
    int mode = number;
    int count = 1;
    int count_mode = 1;
    std::for_each(copy.begin() + 1, copy.end(),
                  [&number,&mode,&count,&count_mode](double x) {
                        if (static_cast<int>(x) == number) {
                            ++count;
                        } else {
                            if (count > count_mode) {
                                count_mode = count;
                                mode = number;
                            }
                            count = 1;
                            number = static_cast<int>(x);
                        }
                  });
    return mode;
}

struct result {
    double avg;
    double stddev;
    double median;
    int mode;
};

// cppcheck-suppress unusedFunction
std::ostream& operator<<(std::ostream& os, const result& r) {
    os << "avg=" << r.avg << ", stddev=" << r.stddev << ", median=" << r.median << ", mode=" << r.mode;
    return os;
}

result aggregate_results(double avg, double stddev, double median, int mode) {
    return {avg, stddev, median, mode};
}

std::shared_ptr<tw::itask<result>> build_graph(std::size_t sample_size, double& alpha, double& beta) {
    auto gen = std::make_shared<std::mt19937>(1);
    auto gen_task = tw::make_task(tw::root, "rand gen", [gen] { return gen; });
    auto size_task = tw::make_task(tw::root, "sample size", [sample_size] { return sample_size; });
    auto alpha_task = tw::make_task(tw::root, "alpha", [&alpha] { return alpha; });
    auto beta_task = tw::make_task(tw::root, "beta", [&beta] { return beta; });

    auto data_task = tw::make_task(tw::consume, "generate gamma", generate_gamma,
                                   size_task, alpha_task, beta_task, gen_task);

    auto avg_task = tw::make_task(tw::consume, "average", average, data_task);
    auto stddev_task = tw::make_task(tw::consume, "stddev", stddev, data_task, avg_task);
    auto median_task = tw::make_task(tw::consume, "median", median, data_task);
    auto mode_task = tw::make_task(tw::consume, "mode", mode, data_task);

    return tw::make_task(tw::consume, "aggregate results", aggregate_results,
                         avg_task, stddev_task, median_task, mode_task);
}

}

namespace examples {

// This example computes statistical key measures from numbers sampled
// from a gamma distribution. The example computes average, standard deviation,
// median, and mode for varying values of alpha and beta.
void statistical_key_facts(std::ostream& os, std::size_t sample_size, bool parallel) {
    os.precision(3);

    double alpha = 1;
    double beta = 1;

    // Building the graph and retrieving the final task
    auto final_task = build_graph(sample_size, alpha, beta);

    // Output the graph for visualization
    const auto graph = final_task->get_graph();
    std::ofstream("statistical_key_facts.dot") << tw::to_string(graph);

    // Creating the executor
    std::shared_ptr<tw::executor> executor;
    if (parallel) {
        executor = std::make_shared<tw::parallel>(4);
    } else {
        executor = std::make_shared<tw::sequential>();
    }

    // Now we start calculating stuff ...
    double count = 1;
    while (count < 4) {
        final_task->schedule_all(*executor);
        const result res = final_task->get_future().get();
        os << res << std::endl;
        // Changing input
        alpha += count;
        beta += count;
        ++count;
    }

}

}

#ifndef UNITTEST
int main() {
    std::cout << "Running example: statistical_key_facts ..." << std::endl;
    examples::statistical_key_facts(std::cout);
}
#endif
