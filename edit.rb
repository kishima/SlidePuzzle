class Result
    attr_accessor:no
    attr_accessor:length
    attr_accessor:time
    attr_accessor:result
    
end

data=[];
File::open(ARGV[0]) {|f|
    f.each {|line|
        if line =~ /result\[(.+)\]:Len=(.+):Time=(.+):(.*)/
            r = Result.new
            r.no    = $1.to_i
            r.length= $2.to_i
            r.time  = $3.to_i
            r.result= $4
            if data[r.no] != nil
                if data[r.no].length==0
                    data[r.no] = r
                elsif data[r.no].length > r.length
                    data[r.no] = r
                    
                end
            else
                data[r.no] = r
            end
        end
    }
}

0.upto(4999){|n|
    if data[n] != nil
        if(data[n].length>0)
            #puts "1"
            #puts n.to_s+","+data[n].result;
            puts data[n].result;
        else
            #puts "0"
            #puts n.to_s+","+data[n].result;
            puts data[n].result;
        end

    end
        
}
