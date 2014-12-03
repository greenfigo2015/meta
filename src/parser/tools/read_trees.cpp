#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "logging/logger.h"
#include "parser/io/ptb_reader.h"
#include "parser/trees/visitors/annotation_remover.h"
#include "parser/trees/visitors/empty_remover.h"
#include "parser/trees/visitors/unary_chain_remover.h"
#include "parser/trees/visitors/multi_transformer.h"
#include "parser/trees/visitors/head_finder.h"
#include "parser/trees/visitors/binarizer.h"

using namespace meta;

struct annotation_checker : public parser::const_visitor<bool>
{
    bool operator()(const parser::leaf_node&) override
    {
        return true;
    }

    bool operator()(const parser::internal_node& inode) override
    {
        if (!inode.head_constituent())
        {
            std::cout << "Node missing head: " << inode.category() << std::endl;
            return false;
        }

        if (!inode.head_lexicon())
        {
            std::cout << "Node missing head lex: " << inode.category()
                      << std::endl;
            return false;
        }

        bool res = true;
        inode.each_child([&](const parser::node* child)
                         {
                             res = res && child->accept(*this);
                         });
        return res;
    }
};

struct binary_checker : public parser::const_visitor<bool>
{
    bool operator()(const parser::leaf_node&) override
    {
        return true;
    }

    bool operator()(const parser::internal_node& inode) override
    {
        if (inode.num_children() > 2)
            return false;

        bool res = true;
        inode.each_child([&](const parser::node* child)
                         {
                             res = res && child->accept(*this);
                         });
        return res;
    }
};

int main(int argc, char** argv)
{

    if (argc < 3)
    {
        std::cerr << "Usage: " << argv[0]
                  << " [options...] trees.mrg [trees2.mrg...]" << std::endl;

        std::cerr << "\noptions:\n\t--print: print the trees while parsing"
                  << std::endl;
        return 1;
    }

    std::vector<std::string> args{argv, argv + argc};
    bool print = std::find(args.begin(), args.end(), "--print") != args.end();

    logging::set_cerr_logging();

    parser::multi_transformer<parser::annotation_remover, parser::empty_remover,
                              parser::unary_chain_remover> transformer;

    parser::head_finder hf;
    annotation_checker ann_check;
    binary_checker bin_check;
    parser::binarizer bin;

    for (size_t arg = 1; arg < args.size(); ++arg)
    {
        if (args[arg] == "--print")
            continue;
        std::cout << "Parsing: " << args[arg] << "..." << std::endl;
        for (auto& tree : parser::io::extract_trees(argv[arg]))
        {
            parser::parse_tree t{std::move(tree)};
            if (print)
            {
                std::cout << "Original: " << std::endl;
                std::cout << t << std::endl;
            }

            t.transform(transformer);
            if (print)
            {
                std::cout << "Transformed: " << std::endl;
                std::cout << t << std::endl;
            }

            t.visit(hf);
            if (!t.visit(ann_check))
                throw std::runtime_error{"Failed to fully annotate heads"};
            t.transform(bin);
            if (!t.visit(bin_check))
                throw std::runtime_error{
                    "Binarizer failed to fully binarize the tree"};
            if (!t.visit(ann_check))
                throw std::runtime_error{
                    "Binarized tree missing head annotations"};

            if (print)
            {
                std::cout << "Binarized: " << std::endl;
                std::cout << t << std::endl;
            }
        }
    }
    return 0;
}
