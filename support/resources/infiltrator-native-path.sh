# InfiltratorOS native PATH.
# Infiltrator applications and commands are discovered from the native
# namespaces; no Calculator executable alias is installed under /usr/bin.
case ":$PATH:" in
  *:/Programs:*) ;;
  *) PATH="/Programs:$PATH" ;;
esac
case ":$PATH:" in
  *:/System/Commands:*) ;;
  *) PATH="/System/Commands:$PATH" ;;
esac
export PATH
