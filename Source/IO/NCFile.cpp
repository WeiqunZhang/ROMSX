#include <string>

#include "NCFile.H"
#include "NCInterface.H"


std::string ReadNetCDFVarAttrStr (const std::string& fname,
                                  const std::string& var_name,
                                  const std::string& attr_name)
{
    std::string attr_val;
    if (amrex::ParallelDescriptor::IOProcessor())
    {
        auto ncf = ncutils::NCFile::open(fname, NC_CLOBBER | NC_NETCDF4);
        attr_val = ncf.var(var_name).get_attr(attr_name);
        ncf.close();
    }
    return attr_val;
}
