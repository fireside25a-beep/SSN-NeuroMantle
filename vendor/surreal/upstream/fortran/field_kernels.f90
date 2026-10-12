module surreal_field_kernels
  use iso_c_binding
  implicit none
contains
  pure integer(c_int) function idx(i,j,k,nx,ny,nz) result(p)
    integer(c_int), value :: i,j,k,nx,ny,nz
    integer(c_int) :: ii,jj,kk
    ii = modulo(i,nx); jj = modulo(j,ny); kk = modulo(k,nz)
    p = 1 + ii + nx*(jj + ny*kk)
  end function idx

  subroutine surreal_laplacian_i64(inp,out,nx,ny,nz) bind(C,name="surreal_laplacian_i64")
    integer(c_int64_t), intent(in) :: inp(*)
    integer(c_int64_t), intent(out) :: out(*)
    integer(c_int), value :: nx,ny,nz
    integer(c_int) :: i,j,k,p
    do k=0,nz-1; do j=0,ny-1; do i=0,nx-1
      p=idx(i,j,k,nx,ny,nz)
      out(p)=inp(idx(i+1,j,k,nx,ny,nz))+inp(idx(i-1,j,k,nx,ny,nz)) &
            +inp(idx(i,j+1,k,nx,ny,nz))+inp(idx(i,j-1,k,nx,ny,nz)) &
            +inp(idx(i,j,k+1,nx,ny,nz))+inp(idx(i,j,k-1,nx,ny,nz))-6_c_int64_t*inp(p)
    end do; end do; end do
  end subroutine

  subroutine surreal_divergence_i64(x,y,z,out,nx,ny,nz) bind(C,name="surreal_divergence_i64")
    integer(c_int64_t), intent(in) :: x(*),y(*),z(*)
    integer(c_int64_t), intent(out) :: out(*)
    integer(c_int), value :: nx,ny,nz
    integer(c_int) :: i,j,k,p
    do k=0,nz-1; do j=0,ny-1; do i=0,nx-1
      p=idx(i,j,k,nx,ny,nz)
      out(p)=(x(idx(i+1,j,k,nx,ny,nz))-x(p)) + &
             (y(idx(i,j+1,k,nx,ny,nz))-y(p)) + &
             (z(idx(i,j,k+1,nx,ny,nz))-z(p))
    end do; end do; end do
  end subroutine

  subroutine surreal_maxwell_leap_i64(ex,ey,ez,bx,by,bz,nx,ny,nz) bind(C,name="surreal_maxwell_leap_i64")
    integer(c_int64_t), intent(inout) :: ex(*),ey(*),ez(*),bx(*),by(*),bz(*)
    integer(c_int), value :: nx,ny,nz
    integer(c_int) :: n,i,j,k,p
    integer(c_int64_t), allocatable :: nex(:),ney(:),nez(:),nbx(:),nby(:),nbz(:)
    integer(c_int64_t) :: cx,cy,cz
    n=nx*ny*nz; allocate(nex(n),ney(n),nez(n),nbx(n),nby(n),nbz(n))
    ! B' = B - curl(E), using forward differences. div(curl)=0 under same commuting differences.
    do k=0,nz-1; do j=0,ny-1; do i=0,nx-1
      p=idx(i,j,k,nx,ny,nz)
      cx=(ez(idx(i,j+1,k,nx,ny,nz))-ez(p))-(ey(idx(i,j,k+1,nx,ny,nz))-ey(p))
      cy=(ex(idx(i,j,k+1,nx,ny,nz))-ex(p))-(ez(idx(i+1,j,k,nx,ny,nz))-ez(p))
      cz=(ey(idx(i+1,j,k,nx,ny,nz))-ey(p))-(ex(idx(i,j+1,k,nx,ny,nz))-ex(p))
      nbx(p)=bx(p)-cx; nby(p)=by(p)-cy; nbz(p)=bz(p)-cz
    end do; end do; end do
    ! E' = E + curl(B'), same exact operator.
    do k=0,nz-1; do j=0,ny-1; do i=0,nx-1
      p=idx(i,j,k,nx,ny,nz)
      cx=(nbz(idx(i,j+1,k,nx,ny,nz))-nbz(p))-(nby(idx(i,j,k+1,nx,ny,nz))-nby(p))
      cy=(nbx(idx(i,j,k+1,nx,ny,nz))-nbx(p))-(nbz(idx(i+1,j,k,nx,ny,nz))-nbz(p))
      cz=(nby(idx(i+1,j,k,nx,ny,nz))-nby(p))-(nbx(idx(i,j+1,k,nx,ny,nz))-nbx(p))
      nex(p)=ex(p)+cx; ney(p)=ey(p)+cy; nez(p)=ez(p)+cz
    end do; end do; end do
    ex(1:n)=nex; ey(1:n)=ney; ez(1:n)=nez; bx(1:n)=nbx; by(1:n)=nby; bz(1:n)=nbz
    deallocate(nex,ney,nez,nbx,nby,nbz)
  end subroutine

  subroutine surreal_kick_drift_i64(pos,vel,acc,dt,n) bind(C,name="surreal_kick_drift_i64")
    integer(c_int64_t), intent(inout) :: pos(*),vel(*)
    integer(c_int64_t), intent(in) :: acc(*)
    integer(c_int64_t), value :: dt
    integer(c_int), value :: n
    integer(c_int) :: i
    do i=1,n
      vel(i)=vel(i)+acc(i)*dt
      pos(i)=pos(i)+vel(i)*dt
    end do
  end subroutine
end module surreal_field_kernels
