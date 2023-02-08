#include "write_HS_sparse.h"
#include "module_hamilt_pw/hamilt_pwdft/global.h"
#include "module_hamilt_lcao/hamilt_lcaodft/global_fp.h"
#include "src_parallel/parallel_reduce.h"
#include "module_base/timer.h"

void ModuleIO::save_HSR_sparse(
    const int &istep,
    LCAO_Matrix &lm,
    const double& sparse_threshold,
    const bool &binary,  
    const std::string &SR_filename, 
    const std::string &HR_filename_up, 
    const std::string &HR_filename_down = ""
)
{
    ModuleBase::TITLE("ModuleIO","save_HSR_sparse");
    ModuleBase::timer::tick("ModuleIO","save_HSR_sparse");

    auto &all_R_coor_ptr = lm.all_R_coor;
    auto &output_R_coor_ptr = lm.output_R_coor;
    auto &HR_sparse_ptr = lm.HR_sparse;
    auto &SR_sparse_ptr = lm.SR_sparse;
    auto &HR_soc_sparse_ptr = lm.HR_soc_sparse;
    auto &SR_soc_sparse_ptr = lm.SR_soc_sparse;

    int total_R_num = all_R_coor_ptr.size();
    int output_R_number = 0;
    int *H_nonzero_num[2] = {nullptr, nullptr};
    int *S_nonzero_num = nullptr;
    int step = istep;

    S_nonzero_num = new int[total_R_num];
    ModuleBase::GlobalFunc::ZEROS(S_nonzero_num, total_R_num);

    int spin_loop = 1;
    if (GlobalV::NSPIN == 2)
    {
        spin_loop = 2;
    }

    for (int ispin = 0; ispin < spin_loop; ++ispin)
    {
        H_nonzero_num[ispin] = new int[total_R_num];
        ModuleBase::GlobalFunc::ZEROS(H_nonzero_num[ispin], total_R_num);
    }

    int count = 0;
    for (auto &R_coor : all_R_coor_ptr)
    {
        if (GlobalV::NSPIN != 4)
        {
            for (int ispin = 0; ispin < spin_loop; ++ispin)
            {
                auto iter = HR_sparse_ptr[ispin].find(R_coor);
                if (iter != HR_sparse_ptr[ispin].end())
                {
                    for (auto &row_loop : iter->second)
                    {
                        H_nonzero_num[ispin][count] += row_loop.second.size();
                    }
                }
            }

            auto iter = SR_sparse_ptr.find(R_coor);
            if (iter != SR_sparse_ptr.end())
            {
                for (auto &row_loop : iter->second)
                {
                    S_nonzero_num[count] += row_loop.second.size();
                }
            }
        }
        else
        {
            auto iter = HR_soc_sparse_ptr.find(R_coor);
            if (iter != HR_soc_sparse_ptr.end())
            {
                for (auto &row_loop : iter->second)
                {
                    H_nonzero_num[0][count] += row_loop.second.size();
                }
            }

            iter = SR_soc_sparse_ptr.find(R_coor);
            if (iter != SR_soc_sparse_ptr.end())
            {
                for (auto &row_loop : iter->second)
                {
                    S_nonzero_num[count] += row_loop.second.size();
                }
            }
        }

        count++;
    }

    Parallel_Reduce::reduce_int_all(S_nonzero_num, total_R_num);
    for (int ispin = 0; ispin < spin_loop; ++ispin)
    {
        Parallel_Reduce::reduce_int_all(H_nonzero_num[ispin], total_R_num);
    }

    if (GlobalV::NSPIN == 2)
    {
        for (int index = 0; index < total_R_num; ++index)
        {
            if (H_nonzero_num[0][index] != 0 || H_nonzero_num[1][index] != 0 || S_nonzero_num[index] != 0)
            {
                output_R_number++;
            }
        }
    }
    else
    {
        for (int index = 0; index < total_R_num; ++index)
        {
            if (H_nonzero_num[0][index] != 0 || S_nonzero_num[index] != 0)
            {
                output_R_number++;
            }
        }
    }

    std::stringstream ssh[2];
    std::stringstream sss;
    if(GlobalV::CALCULATION == "md")
    {
        ssh[0] << GlobalV::global_matrix_dir << istep << "_" << HR_filename_up;
        ssh[1] << GlobalV::global_matrix_dir << istep << "_" << HR_filename_down;
        sss << GlobalV::global_matrix_dir << istep << "_" << SR_filename;
    }
    else
    {
        ssh[0] << GlobalV::global_out_dir << HR_filename_up;
        ssh[1] << GlobalV::global_out_dir << HR_filename_down;
        sss << GlobalV::global_out_dir << SR_filename;
    }
    std::ofstream g1[2];
    std::ofstream g2;

    if(GlobalV::DRANK==0)
    {
        if (binary)
        {
            for (int ispin = 0; ispin < spin_loop; ++ispin)
            {
                g1[ispin].open(ssh[ispin].str().c_str(), ios::binary | ios::app);
                g1[ispin].write(reinterpret_cast<char *>(&step), sizeof(int));
                g1[ispin].write(reinterpret_cast<char *>(&GlobalV::NLOCAL), sizeof(int));
                g1[ispin].write(reinterpret_cast<char *>(&output_R_number), sizeof(int));
            }

            g2.open(sss.str().c_str(), ios::binary | ios::app);
            g2.write(reinterpret_cast<char *>(&step), sizeof(int));
            g2.write(reinterpret_cast<char *>(&GlobalV::NLOCAL), sizeof(int));
            g2.write(reinterpret_cast<char *>(&output_R_number), sizeof(int));
        }
        else
        {
            for (int ispin = 0; ispin < spin_loop; ++ispin)
            {
                g1[ispin].open(ssh[ispin].str().c_str(), ios::app);
                g1[ispin] << "STEP: " << istep << std::endl;
                g1[ispin] << "Matrix Dimension of H(R): " << GlobalV::NLOCAL <<std::endl;
                g1[ispin] << "Matrix number of H(R): " << output_R_number << std::endl;
            }

            g2.open(sss.str().c_str(), ios::app);
            g2 << "STEP: " << istep <<std::endl;
            g2 << "Matrix Dimension of S(R): " << GlobalV::NLOCAL <<std::endl;
            g2 << "Matrix number of S(R): " << output_R_number << std::endl;
        }
    }

    output_R_coor_ptr.clear();

    count = 0;
    for (auto &R_coor : all_R_coor_ptr)
    {
        int dRx = R_coor.x;
        int dRy = R_coor.y;
        int dRz = R_coor.z;

        if (GlobalV::NSPIN == 2)
        {
            if (H_nonzero_num[0][count] == 0 && H_nonzero_num[1][count] == 0 && S_nonzero_num[count] == 0)
            {
                count++;
                continue;
            }
        }
        else
        {
            if (H_nonzero_num[0][count] == 0 && S_nonzero_num[count] == 0)
            {
                count++;
                continue;
            }
        }

        output_R_coor_ptr.insert(R_coor);

        if (GlobalV::DRANK == 0)
        {
            if (binary)
            {
                for (int ispin = 0; ispin < spin_loop; ++ispin)
                {
                    g1[ispin].write(reinterpret_cast<char *>(&dRx), sizeof(int));
                    g1[ispin].write(reinterpret_cast<char *>(&dRy), sizeof(int));
                    g1[ispin].write(reinterpret_cast<char *>(&dRz), sizeof(int));
                    g1[ispin].write(reinterpret_cast<char *>(&H_nonzero_num[ispin][count]), sizeof(int));
                }

                g2.write(reinterpret_cast<char *>(&dRx), sizeof(int));
                g2.write(reinterpret_cast<char *>(&dRy), sizeof(int));
                g2.write(reinterpret_cast<char *>(&dRz), sizeof(int));
                g2.write(reinterpret_cast<char *>(&S_nonzero_num[count]), sizeof(int));
            }
            else
            {
                for (int ispin = 0; ispin < spin_loop; ++ispin)
                {
                    g1[ispin] << dRx << " " << dRy << " " << dRz << " " << H_nonzero_num[ispin][count] << std::endl;
                }
                g2 << dRx << " " << dRy << " " << dRz << " " << S_nonzero_num[count] << std::endl;
            }
        }

        for (int ispin = 0; ispin < spin_loop; ++ispin)
        {
            if (H_nonzero_num[ispin][count] == 0)
            {
                // if (GlobalV::DRANK == 0)
                // {
                //     if (!binary)
                //     {
                //         g1[ispin] << std::endl;
                //         g1[ispin] << std::endl;
                //         for (int index = 0; index < GlobalV::NLOCAL+1; ++index)
                //         {
                //             g1[ispin] << 0 << " ";
                //         }
                //         g1[ispin] << std::endl;
                //     }
                // }
            }
            else
            {
                if (GlobalV::NSPIN != 4)
                {
                    output_single_R(g1[ispin], HR_sparse_ptr[ispin][R_coor], sparse_threshold, binary, *lm.ParaV);
                }
                else
                {
                    output_soc_single_R(g1[ispin], HR_soc_sparse_ptr[R_coor], sparse_threshold, binary, *lm.ParaV);
                }
            }
        }

        if (S_nonzero_num[count] == 0)
        {
            // if (!binary)
            // {
            //     if (GlobalV::DRANK == 0)
            //     {
            //         g2 << std::endl;
            //         g2 << std::endl;
            //         for (int index = 0; index < GlobalV::NLOCAL+1; ++index)
            //         {
            //             g2 << 0 << " ";
            //         }
            //         g2 << std::endl;
            //     }
            // }
        }
        else
        {
            if (GlobalV::NSPIN != 4)
            {
                output_single_R(g2, SR_sparse_ptr[R_coor], sparse_threshold, binary, *lm.ParaV);
            }
            else
            {
                output_soc_single_R(g2, SR_soc_sparse_ptr[R_coor], sparse_threshold, binary, *lm.ParaV);
            }
        }

        count++;

    }

    if(GlobalV::DRANK==0) 
    {
        for (int ispin = 0; ispin < spin_loop; ++ispin) g1[ispin].close();
        g2.close();
    }
    
    for (int ispin = 0; ispin < spin_loop; ++ispin) 
    {
        delete[] H_nonzero_num[ispin];
        H_nonzero_num[ispin] = nullptr;
    }
    delete[] S_nonzero_num;
    S_nonzero_num = nullptr;

    ModuleBase::timer::tick("ModuleIO","save_HSR_sparse");
    return;
}

void ModuleIO::save_SR_sparse(
    LCAO_Matrix &lm,
    const double& sparse_threshold,
    const bool &binary,  
    const std::string &SR_filename
)
{
    ModuleBase::TITLE("ModuleIO","save_SR_sparse");
    ModuleBase::timer::tick("ModuleIO","save_SR_sparse");

    auto &all_R_coor_ptr = lm.all_R_coor;
    auto &SR_sparse_ptr = lm.SR_sparse;
    auto &SR_soc_sparse_ptr = lm.SR_soc_sparse;

    int total_R_num = all_R_coor_ptr.size();
    int output_R_number = 0;
    int *S_nonzero_num = nullptr;

    S_nonzero_num = new int[total_R_num];
    ModuleBase::GlobalFunc::ZEROS(S_nonzero_num, total_R_num);

    int count = 0;
    for (auto &R_coor : all_R_coor_ptr)
    {
        if (GlobalV::NSPIN != 4)
        {
            auto iter = SR_sparse_ptr.find(R_coor);
            if (iter != SR_sparse_ptr.end())
            {
                for (auto &row_loop : iter->second)
                {
                    S_nonzero_num[count] += row_loop.second.size();
                }
            }
        }
        else
        {
            auto iter = SR_soc_sparse_ptr.find(R_coor);
            if (iter != SR_soc_sparse_ptr.end())
            {
                for (auto &row_loop : iter->second)
                {
                    S_nonzero_num[count] += row_loop.second.size();
                }
            }
        }

        count++;
    }

    Parallel_Reduce::reduce_int_all(S_nonzero_num, total_R_num);

    for (int index = 0; index < total_R_num; ++index)
    {
        if (S_nonzero_num[index] != 0)
        {
            output_R_number++;
        }
    }

    std::stringstream sss;
    sss << SR_filename;
    std::ofstream g2;

    if(GlobalV::DRANK==0)
    {
        if (binary)
        {
            g2.open(sss.str().c_str(), ios::binary);
            g2.write(reinterpret_cast<char *>(&GlobalV::NLOCAL), sizeof(int));
            g2.write(reinterpret_cast<char *>(&output_R_number), sizeof(int));
        }
        else
        {
            g2.open(sss.str().c_str());
            g2 << "Matrix Dimension of S(R): " << GlobalV::NLOCAL <<std::endl;
            g2 << "Matrix number of S(R): " << output_R_number << std::endl;
        }
    }

    count = 0;
    for (auto &R_coor : all_R_coor_ptr)
    {
        int dRx = R_coor.x;
        int dRy = R_coor.y;
        int dRz = R_coor.z;

        if (S_nonzero_num[count] == 0)
        {
            count++;
            continue;
        }

        if (GlobalV::DRANK == 0)
        {
            if (binary)
            {
                g2.write(reinterpret_cast<char *>(&dRx), sizeof(int));
                g2.write(reinterpret_cast<char *>(&dRy), sizeof(int));
                g2.write(reinterpret_cast<char *>(&dRz), sizeof(int));
                g2.write(reinterpret_cast<char *>(&S_nonzero_num[count]), sizeof(int));
            }
            else
            {
                g2 << dRx << " " << dRy << " " << dRz << " " << S_nonzero_num[count] << std::endl;
            }
        }

        if (GlobalV::NSPIN != 4)
        {
            output_single_R(g2, SR_sparse_ptr[R_coor], sparse_threshold, binary, *lm.ParaV);
        }
        else
        {
            output_soc_single_R(g2, SR_soc_sparse_ptr[R_coor], sparse_threshold, binary, *lm.ParaV);
        }

        count++;

    }

    if(GlobalV::DRANK==0) 
    {
        g2.close();
    }

    delete[] S_nonzero_num;
    S_nonzero_num = nullptr;

    ModuleBase::timer::tick("ModuleIO","save_SR_sparse");
    return;
}

void ModuleIO::output_single_R(std::ofstream &ofs, const std::map<size_t, std::map<size_t, double>> &XR, const double &sparse_threshold, const bool &binary, const Parallel_Orbitals &pv)
{
    double *line = nullptr;
    std::vector<int> indptr;
    indptr.reserve(GlobalV::NLOCAL + 1);
    indptr.push_back(0);

    std::stringstream tem1;
    tem1 << GlobalV::global_out_dir << "temp_sparse_indices.dat";
    std::ofstream ofs_tem1;
    std::ifstream ifs_tem1;

    if (GlobalV::DRANK == 0)
    {
        if (binary)
        {
            ofs_tem1.open(tem1.str().c_str(), ios::binary);
        }
        else
        {
            ofs_tem1.open(tem1.str().c_str());
        }
    }

    line = new double[GlobalV::NLOCAL];
    for(int row = 0; row < GlobalV::NLOCAL; ++row)
    {
        // line = new double[GlobalV::NLOCAL];
        ModuleBase::GlobalFunc::ZEROS(line, GlobalV::NLOCAL);

        if(pv.trace_loc_row[row] >= 0)
        {
            auto iter = XR.find(row);
            if (iter != XR.end())
            {
                for (auto &value : iter->second)
                {
                    line[value.first] = value.second;
                }
            }
        }

        Parallel_Reduce::reduce_double_all(line, GlobalV::NLOCAL);

        if(GlobalV::DRANK == 0)
        {
            int nonzeros_count = 0;
            for (int col = 0; col < GlobalV::NLOCAL; ++col)
            {
                if (std::abs(line[col]) > sparse_threshold)
                {
                    if (binary)
                    {
                        ofs.write(reinterpret_cast<char *>(&line[col]), sizeof(double));
                        ofs_tem1.write(reinterpret_cast<char *>(&col), sizeof(int));
                    }
                    else
                    {
                        ofs << " " << fixed << scientific << std::setprecision(8) << line[col];
                        ofs_tem1 << " " << col;
                    }

                    nonzeros_count++;

                }

            }
            nonzeros_count += indptr.back();
            indptr.push_back(nonzeros_count);
        }

        // delete[] line;
        // line = nullptr;

    }

    delete[] line;
    line = nullptr;

    if (GlobalV::DRANK == 0)
    {
        if (binary)
        {
            ofs_tem1.close();
            ifs_tem1.open(tem1.str().c_str(), ios::binary);
            ofs << ifs_tem1.rdbuf();
            ifs_tem1.close();
            for (auto &i : indptr)
            {
                ofs.write(reinterpret_cast<char *>(&i), sizeof(int));
            }
        }
        else
        {
            ofs << std::endl;
            ofs_tem1 << std::endl;
            ofs_tem1.close();
            ifs_tem1.open(tem1.str().c_str());
            ofs << ifs_tem1.rdbuf();
            ifs_tem1.close();
            for (auto &i : indptr)
            {
                ofs << " " << i;
            }
            ofs << std::endl;
        }

        std::remove(tem1.str().c_str());

    }

}

void ModuleIO::output_soc_single_R(std::ofstream &ofs, const std::map<size_t, std::map<size_t, std::complex<double>>> &XR, const double &sparse_threshold, const bool &binary, const Parallel_Orbitals &pv)
{
    std::complex<double> *line = nullptr;
    std::vector<int> indptr;
    indptr.reserve(GlobalV::NLOCAL + 1);
    indptr.push_back(0);

    std::stringstream tem1;
    tem1 << GlobalV::global_out_dir << "temp_sparse_indices.dat";
    std::ofstream ofs_tem1;
    std::ifstream ifs_tem1;

    if (GlobalV::DRANK == 0)
    {
        if (binary)
        {
            ofs_tem1.open(tem1.str().c_str(), ios::binary);
        }
        else
        {
            ofs_tem1.open(tem1.str().c_str());
        }
    }

    line = new std::complex<double>[GlobalV::NLOCAL];
    for(int row = 0; row < GlobalV::NLOCAL; ++row)
    {
        // line = new std::complex<double>[GlobalV::NLOCAL];
        ModuleBase::GlobalFunc::ZEROS(line, GlobalV::NLOCAL);

        if(pv.trace_loc_row[row] >= 0)
        {
            auto iter = XR.find(row);
            if (iter != XR.end())
            {
                for (auto &value : iter->second)
                {
                    line[value.first] = value.second;
                }
            }
        }

        Parallel_Reduce::reduce_complex_double_all(line, GlobalV::NLOCAL);

        if (GlobalV::DRANK == 0)
        {
            int nonzeros_count = 0;
            for (int col = 0; col < GlobalV::NLOCAL; ++col)
            {
                if (std::abs(line[col]) > sparse_threshold)
                {
                    if (binary)
                    {
                        ofs.write(reinterpret_cast<char *>(&line[col]), sizeof(std::complex<double>));
                        ofs_tem1.write(reinterpret_cast<char *>(&col), sizeof(int));
                    }
                    else
                    {
                        ofs << " (" << fixed << scientific << std::setprecision(8) << line[col].real() << "," 
                                    << fixed << scientific << std::setprecision(8) << line[col].imag() << ")";
                        ofs_tem1 << " " << col;
                    }

                    nonzeros_count++;

                }

            }
            nonzeros_count += indptr.back();
            indptr.push_back(nonzeros_count);
        }

        // delete[] line;
        // line = nullptr;

    }

    delete[] line;
    line = nullptr;

    if (GlobalV::DRANK == 0)
    {
        if (binary)
        {
            ofs_tem1.close();
            ifs_tem1.open(tem1.str().c_str(), ios::binary);
            ofs << ifs_tem1.rdbuf();
            ifs_tem1.close();
            for (auto &i : indptr)
            {
                ofs.write(reinterpret_cast<char *>(&i), sizeof(int));
            }
        }
        else
        {
            ofs << std::endl;
            ofs_tem1 << std::endl;
            ofs_tem1.close();
            ifs_tem1.open(tem1.str().c_str());
            ofs << ifs_tem1.rdbuf();
            ifs_tem1.close();
            for (auto &i : indptr)
            {
                ofs << " " << i;
            }
            ofs << std::endl;
        }

        std::remove(tem1.str().c_str());
    }

}